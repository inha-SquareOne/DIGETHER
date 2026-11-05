#include "Game/Save/DigetherSaveTypes.h"

const TCHAR* LexToString(const EDigetherSaveResult Result)
{
	switch (Result)
	{
	case EDigetherSaveResult::Success:
		return TEXT("Success");
	case EDigetherSaveResult::NotAuthority:
		return TEXT("NotAuthority");
	case EDigetherSaveResult::HostIdentityMissing:
		return TEXT("HostIdentityMissing");
	case EDigetherSaveResult::HostIdentityMismatch:
		return TEXT("HostIdentityMismatch");
	case EDigetherSaveResult::NotFound:
		return TEXT("NotFound");
	case EDigetherSaveResult::UnsupportedVersion:
		return TEXT("UnsupportedVersion");
	case EDigetherSaveResult::CorruptData:
		return TEXT("CorruptData");
	case EDigetherSaveResult::InvalidState:
		return TEXT("InvalidState");
	case EDigetherSaveResult::UnsafePhase:
		return TEXT("UnsafePhase");
	case EDigetherSaveResult::AlreadySettled:
		return TEXT("AlreadySettled");
	case EDigetherSaveResult::PlayerNotFound:
		return TEXT("PlayerNotFound");
	case EDigetherSaveResult::PlayerIdentityMismatch:
		return TEXT("PlayerIdentityMismatch");
	case EDigetherSaveResult::UnsupportedIdentity:
		return TEXT("UnsupportedIdentity");
	case EDigetherSaveResult::IoFailure:
		return TEXT("IoFailure");
	}
	return TEXT("Unknown");
}
