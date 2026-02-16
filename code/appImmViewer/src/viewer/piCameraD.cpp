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
	// x = yaw (rotation around Y axis)
	// y = pitch (rotation around X axis)
	// Ensure zero roll by reconstructing camera matrix from position and direction
	
	// Get current position
	vec3d pos = GetPos();
	
	// Get current direction and convert to angles
	vec3d dir = GetDir();
	double yaw = std::atan2(dir.x, dir.z);  // Rotation around Y
	double pitch = std::asin(-dir.y);        // Rotation around X (clamped)
	
	// Apply deltas
	yaw += x;
	pitch += y;
	
	// Clamp pitch to prevent looking too far up/down (e.g., -85 to 85 degrees)
	const double maxPitch = 3.14159265359 * 0.48; // ~86 degrees
	if (pitch > maxPitch) pitch = maxPitch;
	if (pitch < -maxPitch) pitch = -maxPitch;
	
	// Calculate new direction from yaw and pitch (no roll possible)
	vec3d newDir;
	newDir.x = std::sin(yaw) * std::cos(pitch);
	newDir.y = -std::sin(pitch);
	newDir.z = std::cos(yaw) * std::cos(pitch);
	
	// Reconstruct camera matrix with world up (0,1,0) - ensures zero roll
	mWorldToCamera = setLookat(pos, pos + newDir, vec3d(0.0, 1.0, 0.0));
	mCameraToWorld = invert(mWorldToCamera);
}

}
