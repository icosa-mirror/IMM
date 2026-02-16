#include <math.h>
#include "libImmCore/src/libBasics/piVecTypes.h"
#include "piCameraD.h"

namespace ImmCore {

piCameraD::piCameraD()
{
	mWorldToCamera = mat4x4d::identity();
	mCameraToWorld = mat4x4d::identity();
}

piCameraD::~piCameraD()
{
}

void piCameraD::Set(const vec3d & pos, const vec3d & dir, const double roll)
{
	mWorldToCamera = setLookat( pos, pos + dir, vec3d(std::sin(roll), std::cos(roll), std::sin(roll) ));
	mCameraToWorld = invert(mWorldToCamera);
}

void piCameraD::SetPos(const vec3d & pos)
{
	mWorldToCamera[ 3] = -(mWorldToCamera[0] * pos[0] + mWorldToCamera[1] * pos[1] + mWorldToCamera[ 2] * pos[2]);
	mWorldToCamera[ 7] = -(mWorldToCamera[4] * pos[0] + mWorldToCamera[5] * pos[1] + mWorldToCamera[ 6] * pos[2]);
	mWorldToCamera[11] = -(mWorldToCamera[8] * pos[0] + mWorldToCamera[9] * pos[1] + mWorldToCamera[10] * pos[2]);
	mCameraToWorld = invert(mWorldToCamera);
}

vec3d piCameraD::GetPos( void ) const
{
    return (mCameraToWorld * vec4d(0.0, 0.0, 0.0, 1.0)).xyz();
}

vec3d piCameraD::GetDir(void) const
{
    return normalize((mCameraToWorld * vec4d(0.0, 0.0, -1.0, 0.0)).xyz());
}

const mat4x4d & piCameraD::GetWorldToCamera(void) const
{
    return mWorldToCamera;
}

const mat4x4d & piCameraD::GetCameraToWorld(void) const
{
    return mCameraToWorld;
}

void piCameraD::SetWorldToCamera(const mat4x4d & mat)
{
	mWorldToCamera = mat;
	mCameraToWorld = invert(mWorldToCamera);
}

void piCameraD::LocalMove( const vec3d & pos )
{
	mWorldToCamera[ 3] -= pos.x;
	mWorldToCamera[ 7] -= pos.y;
	mWorldToCamera[11] -= pos.z;
	mCameraToWorld = invert(mWorldToCamera);
}

void piCameraD::GlobalMove(const vec3d & pos)
{
	mWorldToCamera[ 3] -= (mWorldToCamera[0] * pos[0] + mWorldToCamera[1] * pos[1] + mWorldToCamera[ 2] * pos[2]);
	mWorldToCamera[ 7] -= (mWorldToCamera[4] * pos[0] + mWorldToCamera[5] * pos[1] + mWorldToCamera[ 6] * pos[2]);
	mWorldToCamera[11] -= (mWorldToCamera[8] * pos[0] + mWorldToCamera[9] * pos[1] + mWorldToCamera[10] * pos[2]);
	mCameraToWorld = invert(mWorldToCamera);
}


void piCameraD::RotateXY(const double x, const double y )
{
	// Incremental rotation preserving the existing matrix (no setLookat rebuild).
	// Yaw: post-multiply by Ry(x) = rotation around world Y axis
	// Pitch: post-multiply by rotation around horizontal right axis
	//   (derived from forward direction, perpendicular to view in horizontal plane)
	vec3d pos = GetPos();

	// Extract 3x3 rotation part (row-major, includes uniform scale)
	double r00 = mWorldToCamera[0], r01 = mWorldToCamera[1], r02 = mWorldToCamera[2];
	double r10 = mWorldToCamera[4], r11 = mWorldToCamera[5], r12 = mWorldToCamera[6];
	double r20 = mWorldToCamera[8], r21 = mWorldToCamera[9], r22 = mWorldToCamera[10];

	// Step 1: Yaw - post-multiply by Ry(x), affects columns 0 and 2
	double cy = std::cos(x), sy = -std::sin(x);
	double m00 = r00*cy + r02*sy,  m02 = -r00*sy + r02*cy;
	double m10 = r10*cy + r12*sy,  m12 = -r10*sy + r12*cy;
	double m20 = r20*cy + r22*sy,  m22 = -r20*sy + r22*cy;
	// Column 1 unchanged: r01, r11, r21

	// Step 2: Pitch around horizontal right axis (perpendicular to horizontal forward)
	// Forward = -(Row2) = -(m20, r21, m22). Horizontal right = (m22, 0, -m20) normalized.
	double hlen = ::sqrt(m20*m20 + m22*m22);
	if (hlen > 1e-10)
	{
		double ax = m22 / hlen;
		double az = -m20 / hlen;
		double cp = std::cos(y), sp = std::sin(y);
		double omc = 1.0 - cp;

		// Rodrigues rotation matrix around (ax, 0, az):
		double p00 = cp + ax*ax*omc;
		double p01 = -az*sp;
		double p02 = ax*az*omc;
		double p10 = az*sp;
		double p11 = cp;
		double p12 = -ax*sp;
		double p20 = ax*az*omc;
		double p21 = ax*sp;
		double p22 = cp + az*az*omc;

		// Post-multiply: R_final[i] = R_yawed[i] * R_pitch
		double t0, t1, t2;

		t0 = m00; t1 = r01; t2 = m02;
		m00 = t0*p00 + t1*p10 + t2*p20;
		r01 = t0*p01 + t1*p11 + t2*p21;
		m02 = t0*p02 + t1*p12 + t2*p22;

		t0 = m10; t1 = r11; t2 = m12;
		m10 = t0*p00 + t1*p10 + t2*p20;
		r11 = t0*p01 + t1*p11 + t2*p21;
		m12 = t0*p02 + t1*p12 + t2*p22;

		t0 = m20; t1 = r21; t2 = m22;
		m20 = t0*p00 + t1*p10 + t2*p20;
		r21 = t0*p01 + t1*p11 + t2*p21;
		m22 = t0*p02 + t1*p12 + t2*p22;
	}

	// Store rotation
	mWorldToCamera[0] = m00; mWorldToCamera[1] = r01; mWorldToCamera[2] = m02;
	mWorldToCamera[4] = m10; mWorldToCamera[5] = r11; mWorldToCamera[6] = m12;
	mWorldToCamera[8] = m20; mWorldToCamera[9] = r21; mWorldToCamera[10] = m22;

	// Recompute translation from position
	mWorldToCamera[3]  = -(m00*pos.x + r01*pos.y + m02*pos.z);
	mWorldToCamera[7]  = -(m10*pos.x + r11*pos.y + m12*pos.z);
	mWorldToCamera[11] = -(m20*pos.x + r21*pos.y + m22*pos.z);

	mCameraToWorld = invert(mWorldToCamera);
}

}
