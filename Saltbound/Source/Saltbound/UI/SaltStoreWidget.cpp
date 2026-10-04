// Saltbound - a store's screen (D52): the satchel beside a store (the boat's barrel or crate). Functional first; the
// owner gives the design later.

#include "UI/SaltStoreWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/SaltItems.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltSatchelWidget.h"
#include "UI/SaltUIStyle.h"

using namespace SaltUI;

USaltStoreWidget::USaltStoreWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void USaltStoreWidget::SetInventories(USaltInventoryComponent* InSatchel, USaltInventoryComponent* InStore)
{
	// Set before the screen is shown (it binds to both when constructed, and is made anew for every store).
	Satchel = InSatchel;
	Store = InStore;
}

TSharedRef<SWidget> USaltStoreWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.35f));
		Backdrop->SetHorizontalAlignment(HAlign_Center);
		Backdrop->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Backdrop;
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(1060.f);
		Size->SetHeightOverride(580.f);
		Backdrop->SetContent(Size);
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrush(BoxBrush(Tex(TEXT("T_UI_Frame")), FVector2D(256.f, 256.f)));
		Frame->SetPadding(FMargin(30.f));
		Size->SetContent(Frame);
		UBorder* Paper = WidgetTree->ConstructWidget<UBorder>();
		Paper->SetBrush(ImageBrush(Tex(TEXT("T_UI_Parchment")), FVector2D(512.f, 512.f), true));
		Paper->SetPadding(FMargin(22.f, 14.f, 22.f, 16.f));
		Frame->SetContent(Paper);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Paper->SetContent(Column);

		UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>();
		Column->AddChildToVerticalBox(Head)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		Title = Text(WidgetTree, FText::GetEmpty(), 26, Ink, true);
		Head->AddChildToHorizontalBox(Title);
		UTextBlock* Hint = Text(WidgetTree, NSLOCTEXT("Saltbound", "StoreHint", "Click a thing to move it     E / Esc  close"), 13, InkSoft);
		if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(Hint))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Column->AddChildToVerticalBox(Row);
		auto Grid = [this](UHorizontalBox* Into, const FText& Label, int32 Count, int32 Columns, TArray<TObjectPtr<USaltItemSlotWidget>>& Out,
			bool bStoreSide, TObjectPtr<UTextBlock>* OutLabel)
		{
			UVerticalBox* Side = WidgetTree->ConstructWidget<UVerticalBox>();
			if (UHorizontalBoxSlot* S = Into->AddChildToHorizontalBox(Side))
			{
				S->SetPadding(FMargin(0.f, 0.f, 34.f, 0.f));
			}
			UTextBlock* L = Text(WidgetTree, Label, 18, Ink, true);
			Side->AddChildToVerticalBox(L)->SetPadding(FMargin(4.f, 0.f, 0.f, 6.f));
			if (OutLabel)
			{
				*OutLabel = L;
			}
			UUniformGridPanel* G = WidgetTree->ConstructWidget<UUniformGridPanel>();
			G->SetSlotPadding(FMargin(5.f));
			Side->AddChildToVerticalBox(G);
			Out.Reset();
			for (int32 I = 0; I < Count; ++I)
			{
				USaltItemSlotWidget* W = CreateWidget<USaltItemSlotWidget>(this, USaltItemSlotWidget::StaticClass());
				if (bStoreSide)
				{
					W->Setup(I, [this](int32 SlotIndex) { ClickStoreSlot(SlotIndex); }, [this](int32 SlotIndex) { ShowItem(Store.Get(), SlotIndex); });
				}
				else
				{
					W->Setup(I, [this](int32 SlotIndex) { ClickSatchelSlot(SlotIndex); }, [this](int32 SlotIndex) { ShowItem(Satchel.Get(), SlotIndex); });
				}
				Out.Add(W);
				G->AddChildToUniformGrid(W, I / Columns, I % Columns);
			}
		};
		Grid(Row, NSLOCTEXT("Saltbound", "SatchelLabel", "Satchel"), USaltInventoryComponent::Capacity, 6, SatchelSlots, false, nullptr);
		const int32 StoreSize = Store.IsValid() ? Store->GetCapacity() : 10;
		Grid(Row, FText::GetEmpty(), StoreSize, 5, StoreSlots, true, &StoreLabel);

		ItemLine = Text(WidgetTree, FText::GetEmpty(), 15, InkSoft);
		ItemLine->SetAutoWrapText(true);
		Column->AddChildToVerticalBox(ItemLine)->SetPadding(FMargin(4.f, 10.f, 4.f, 0.f));
	}
	return Super::RebuildWidget();
}

void USaltStoreWidget::Bind(bool bBind)
{
	USaltInventoryComponent* S = Satchel.Get();
	USaltInventoryComponent* T = Store.Get();
	if (bBind)
	{
		if (S)
		{
			SatchelChanged = S->OnChanged.AddUObject(this, &USaltStoreWidget::Refresh);
		}
		if (T)
		{
			StoreChanged = T->OnChanged.AddUObject(this, &USaltStoreWidget::Refresh);
		}
	}
	else
	{
		if (S)
		{
			S->OnChanged.Remove(SatchelChanged);
		}
		if (T)
		{
			T->OnChanged.Remove(StoreChanged);
		}
	}
}

void USaltStoreWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Bind(true);
	Refresh();
}

void USaltStoreWidget::NativeDestruct()
{
	Bind(false);
	Super::NativeDestruct();
}

void USaltStoreWidget::Refresh()
{
	auto Fill = [](const USaltInventoryComponent* Inv, TArray<TObjectPtr<USaltItemSlotWidget>>& Slots)
	{
		for (int32 I = 0; I < Slots.Num(); ++I)
		{
			const FSaltItemStack S = Inv && Inv->GetSlots().IsValidIndex(I) ? Inv->GetSlots()[I] : FSaltItemStack();
			const FSaltItemDef* Def = S.IsEmpty() ? nullptr : SaltItems::Find(S.Id);
			Slots[I]->Show(Def && Def->Icon ? LoadObject<UTexture2D>(nullptr, Def->Icon) : nullptr, Def ? S.Count : 0, false);
		}
	};
	Fill(Satchel.Get(), SatchelSlots);
	Fill(Store.Get(), StoreSlots);
	if (Title && Store.IsValid())
	{
		Title->SetText(Store->GetDisplayName());
		StoreLabel->SetText(Store->GetDisplayName());
	}
}

void USaltStoreWidget::ShowItem(const USaltInventoryComponent* From, int32 Index)
{
	const FSaltItemStack S = From && From->GetSlots().IsValidIndex(Index) ? From->GetSlots()[Index] : FSaltItemStack();
	const FSaltItemDef* Def = S.IsEmpty() ? nullptr : SaltItems::Find(S.Id);
	if (ItemLine)
	{
		ItemLine->SetText(Def ? FText::Format(NSLOCTEXT("Saltbound", "StoreItemLine", "{0} ({1}): {2}"), Def->Name, FText::AsNumber(S.Count), Def->Description)
			: FText::GetEmpty());
	}
}

void USaltStoreWidget::ClickSatchelSlot(int32 Index)
{
	if (USaltInventoryComponent* S = Satchel.Get())
	{
		S->MoveStack(Index, Store.Get());
	}
}

void USaltStoreWidget::ClickStoreSlot(int32 Index)
{
	if (USaltInventoryComponent* T = Store.Get())
	{
		T->MoveStack(Index, Satchel.Get());
	}
}

FReply USaltStoreWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::E || Key == EKeys::Escape || Key == EKeys::Tab || Key == EKeys::F10 || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(GetOwningPlayer()))
		{
			PC->CloseStore();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
