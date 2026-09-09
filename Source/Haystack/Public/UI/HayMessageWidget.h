// Copyright (c) 2026 Vanan Andreas.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HayMessageWidget.generated.h"

class UTextBlock;

/**
 * One line of text the HUD fills in. Used for the interact prompt and the needle found message.
 */
UCLASS(Abstract)
class UHayMessageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMessage(const FText& Text);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Message = nullptr;
};
