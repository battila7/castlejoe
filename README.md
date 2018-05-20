# Castlejoe

Header-only compute shader-based curve library. 

## Current Status

This project (at least as of now) is just a proof-of-concept. No error checking is made and only cubic B-Spline curves can be created.

## What's with the name?

It's a broken version of *Casteljau* (which can be familiar from *de Casteljau's algorithm*). A friend once forgot the real name and misspelled it as *Castlejoe*.

## Usage

Prior to making calls to Castlejoe, make sure, that you've opened an OpenGL context (at least OpenGL 4.3 is required).

Note, that you can use your own Point or Vector struct/class, but in that case, you have to supply an appropriate converter. For example:

~~~~C++
struct Position
{
	float x, y, z, w;
};

template <>
class castlejoe::point::Converter<Position> {
public:
	static castlejoe::point::Point convertFrom(const Position &p) {
		return { p.x, p.y, p.z, p.w };
	}

	static Position convertTo(const castlejoe::point::Point &p) {
		return { p.x, p.y, p.z, p.w };
	}
};
~~~~

### Cubic B-Spline

Evaluating a cubic B-Spline curve is as simple as:

~~~~C++
    // Let's assume, that we have a working OpenGL context.

    // We will use castlejoe::point::Point to keep things simple.
    // A ControlPointContext can be shared between curves, so that multiple curves can be
    // fitted to the same set of control points.
    castlejoe::point::ControlPointContext<castlejoe::point::Point> controlPointContext;

    castlejoe::curve::CubicBSpline<castlejoe::point::Point> cubicBSpline(controlPointContext);

    // Create a vector of control points.
    std::vector<castlejoe::point::Point> controlPoints = { { 0, 0, 0, 1 }, { 0, 300, 0, 1 }, { 300, 300, 0, 1 }, { 300, 0, 0, 1 } };

    // This call copies the control point data to the control point buffer.
    controlPointContext.setPoints(controlPoints);

    // evaluateCurve() calculates the curve points but does not copy the result back to the main memory.
    Evaluation<castlejoe::point::Point> eval = cubicBSpline.evaluateCurve();

    // If you want to access the buffer object with the curve points, use eval.buffer.
    // Otherwise, they can be copied to the main memory:
    std::vector<castlejoe::point::Point> curvePoints = eval.extractPoints();
~~~~
