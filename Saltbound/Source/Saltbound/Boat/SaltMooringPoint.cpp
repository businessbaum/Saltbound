// Saltbound - a place the player's boat can moor (D46, D62).

#include "Boat/SaltMooringPoint.h"

FSaltMooring USaltMooringPointComponent::GetMooring() const
{
	FSaltMooring M;
	M.Id = MooringId.IsNone() ? FName(*GetPathName()) : MooringId;
	const FTransform T = GetComponentTransform();
	M.Pose = FTransform(FRotator(0.0, T.Rotator().Yaw, 0.0), T.GetLocation());
	M.Reach = Reach;
	M.PierHeight = PierHeight;
	return M;
}
