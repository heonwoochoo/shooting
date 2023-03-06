#pragma once

UENUM()
enum class EAmmoType : uint8
{
	EAT_9mm UMETA(DisplayName = "9mm"),
	EAT_AR UMETA(DisplayName = "AssultRifle"),

	EAT_MAX UMETA(DisplayName = "Default MAX")
};