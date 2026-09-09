// Copyright (c) 2026 Vanan Andreas.

#include "UI/HayMessageWidget.h"

#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HayMessageWidget)

void UHayMessageWidget::SetMessage(const FText& Text)
{
	Message->SetText(Text);
}
