#ifndef CASTLEJOE_H
#define CASTLEJOE_H

#include <array>
#include <utility>
#include <vector>

namespace castlejoe {
    namespace shaders {
        namespace source {
            const char *CubicBSpline = R"SH_SRC(
                #version 430
                #extension GL_ARB_compute_shader : enable
                #extension GL_ARB_shader_storage_buffer_object : enable

                layout(std140, binding = 0) buffer Ctp {
                    vec4 ControlPoints[];
                };

                layout(std140, binding = 1) buffer Output {
                    vec4 CurvePoints[];
                };

                layout(local_size_x = 101, local_size_y = 1, local_size_z = 1) in;

                shared mat4 geometry;
                shared mat4 coefficients;

                void main() {
                    if (gl_LocalInvocationID.x == 0) {
                        coefficients = (1.0 / 6.0) * mat4(
                            -1,  3, -3, 1,
                            3, -6,  3, 0,
                            -3,  0,  3, 0,
                            1,  4,  1, 0
                        );

                        uint pointStartIndex = gl_WorkGroupID.x;

                        geometry = mat4(
                            ControlPoints[pointStartIndex],
                            ControlPoints[pointStartIndex + 1],
                            ControlPoints[pointStartIndex + 2],
                            ControlPoints[pointStartIndex + 3]
                        );
                    }

                    memoryBarrierShared();
                    barrier();

                    float t = gl_LocalInvocationID.x * 0.01;
                    float t2 = t * t;
                    float t3 = t2 * t;

                    vec4 paramVec = vec4(t3, t2, t, 1);

                    CurvePoints[gl_GlobalInvocationID.x] = geometry * coefficients * paramVec;
                }
            )SH_SRC"; /* CubicBSpline */
        }

        enum Type {
            CUBIC_B_SPLINE = 0
        };

        class Shaders {
        public:
            Shaders() = delete;

            static GLuint getProgram(const Type type) {
                static std::vector<GLuint> programs = initializeProgams();

                return programs[type];
            }

        private:
            static constexpr int SHADER_COUNT = 1;

            static std::vector<GLuint> initializeProgams() {
                std::array<const char *, SHADER_COUNT> shaderSourceMapping = { source::CubicBSpline };

                std::vector<GLuint> programs;

                for (const char *source : shaderSourceMapping) {
                    const GLchar *glc = (const GLchar *)source;
                    GLuint sh = glCreateShader(GL_COMPUTE_SHADER);

                    glShaderSource(sh, 1, &glc, NULL);
                    glCompileShader(sh);

                    GLuint program = glCreateProgram();
                    glAttachShader(program, sh);
                    glLinkProgram(program);

                    programs.push_back(program);
                }

                return programs;
            }
        };
    }

    namespace point {
        struct Point {
            float x, y, z, w;
        };

        template <typename PointType>
        class Converter {
        public:
            Converter() = delete;
        
            static Point convertFrom(const PointType &p) = 0;
            static PointType convertTo(const Point &p) = 0;
        };

        template <>
        class Converter<Point> {
        public:
            static Point convertFrom(const Point &p) {
                return p;
            }

            static Point convertTo(const Point &p) {
                return p;
            }
        };

        template <typename PointType>
        class ControlPointContext {
        public:
            ControlPointContext() {
                glGenBuffers(1, &pointBuffer);
            }

            void setPoints(const std::vector<PointType> points) {
                this->points.clear();

                this->points.reserve(points.size());

                for (const auto &p : points) {
                    this->points.push_back(Converter<PointType>::convertFrom(p));
                }

                fillBuffer();
            }

            size_t getPointCount() const {
                return points.size();
            }

            std::vector<Point> getPoints() const {
                return points;
            }

            GLuint getPointBuffer() const {
                return pointBuffer;
                
            }
        private:
            void fillBuffer() {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, pointBuffer);
                glBufferData(GL_SHADER_STORAGE_BUFFER, points.size() * sizeof Point, NULL, GL_STATIC_DRAW);
                GLint bufMask = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
                Point *bufferPoints = (Point *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, points.size() * sizeof Point, bufMask);
                for (int i = 0; i < points.size(); ++i) {
                    bufferPoints[i] = points[i];
                }
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            }

            std::vector<Point> points;

            GLuint pointBuffer;
        };
    }

    namespace curve {
        template <typename PointType>
        struct Evaluation {
            GLuint buffer;
            GLuint pointCount;

            std::vector<PointType> extractPoints() const {
                std::vector<PointType> curvePoints;
                curvePoints.reserve(pointCount);

                glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
                point::Point *outPoints = (point::Point *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, pointCount * sizeof(point::Point), GL_MAP_READ_BIT);
                for (int i = 0; i < pointCount; ++i) {
                    curvePoints.push_back(point::Converter<PointType>::convertTo(outPoints[i]));
                }
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

                return curvePoints;
            }
        };

        template <typename PointType>
        class Curve {
        public:
            Curve(const point::ControlPointContext<PointType> & controlPointContext):
                controlPointContext(controlPointContext) 
            {}

            virtual Evaluation<PointType> evaluateCurve() = 0;
        protected:
            const point::ControlPointContext<PointType> & controlPointContext;
        };

        template <typename PointType>
        class CubicBSpline : public Curve<PointType> {
        public:
            CubicBSpline(const point::ControlPointContext<PointType> & controlPointContext) : Curve<PointType>(controlPointContext) {
                glGenBuffers(1, &controlPointBuffer);
                glGenBuffers(1, &curvePointBuffer);
            }

            ~CubicBSpline() {

            }

            virtual Evaluation<PointType> evaluateCurve() override {
                if (this->controlPointContext.getPointCount() < 4) {
                    return {};
                }

                int groupCount = controlPoints.size() - 3;

                GLuint outputCount = groupCount * 101;

                glBindBuffer(GL_SHADER_STORAGE_BUFFER, curvePointBuffer);
                glBufferData(GL_SHADER_STORAGE_BUFFER, outputCount * sizeof point::Point, NULL, GL_STATIC_DRAW);

                glUseProgram(shaders::Shaders::getProgram(shaders::Type::CUBIC_B_SPLINE));
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->controlPointContext.getPointBuffer());
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, curvePointBuffer);

                glDispatchCompute(groupCount, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                return Evaluation<PointType>{
                    curvePointBuffer, outputCount
                };
            }
        private:
            GLuint controlPointBuffer;
            GLuint curvePointBuffer;
        };
    }
}

#endif /* CASTLEJOE_H */
