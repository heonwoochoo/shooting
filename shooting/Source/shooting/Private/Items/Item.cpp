// Fill out your copyright notice in the Description page of Project Settings.
#include "Items/Item.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"

AItem::AItem()
{
 	PrimaryActorTick.bCanEverTick = true;

	ItemMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ItemMesh"));
	SetRootComponent(ItemMesh);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(ItemMesh);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(GetRootComponent());
}

void AItem::BeginPlay()
{
	Super::BeginPlay();
	
	// Hide Pickup widget
	PickupWidget->SetVisibility(false);
}

void AItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AItem::SetPickupWidgetVisible(bool isVisible)
{
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(isVisible);
	}
}

