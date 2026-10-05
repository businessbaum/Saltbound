// Saltbound - the satchel (D50, restyled D55, UI_Design.md section 5). The map left it (D57: the chart is on the mast).

#include "UI/SaltSatchelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/SaltItems.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "Styling/CoreStyle.h"
#include "UI/SaltUIStyle.h"
#include "World/SaltPickup.h"

using namespace SaltUI;

namespace
{
	// Pocket layout at 1080p (section 5): 8 x 3, centres x = 401 + 124 col, y = 616 + 140 row; the tag's hole at (1498, 540).
	FVector2D PocketCentre(int32 I) { return FVector2D(401.f + 124.f * (I % 8), 616.f + 140.f * (I / 8)); }
	const FVector2D TagHole(1498.f, 540.f);
	const FVector2D TagHoleInFrame(190.f, 80.f);   // T_UI_Tag is 400 x 600 with its hole here
	constexpr float TagRest = 4.f;                 // degrees
}

// --- Pocket -------------------------------------------------------------------------------------------------------

void USaltItemSlotWidget::Setup(int32 InIndex, TFunction<void(int32)> InOnClicked, TFunction<void(int32)> InOnHovered, float InRadius)
{
	Index = InIndex;
	OnClickedFn = MoveTemp(InOnClicked);
	OnHoveredFn = MoveTemp(InOnHovered);
	Radius = InRadius;
}

void USaltItemSlotWidget::Place(UWidget* W, FVector2D Centre, FVector2D Size)
{
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
	{
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetPosition(FVector2D(2.f * Radius, 2.f * Radius) + Centre);
		S->SetSize(Size);
	}
}

TSharedRef<SWidget> USaltItemSlotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		const float K = Radius / 50.f;
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(4.f * Radius);
		Box->SetHeightOverride(4.f * Radius);
		WidgetTree->RootWidget = Box;
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		Box->AddChild(Root);
		auto Img = [this](const TCHAR* Texture, FVector2D Size)
		{
			UImage* I = WidgetTree->ConstructWidget<UImage>();
			if (Texture)
			{
				I->SetBrush(ImageBrush(Tex(Texture), Size));
			}
			I->SetVisibility(ESlateVisibility::HitTestInvisible);
			Root->AddChildToCanvas(I);
			return I;
		};
		Glow = Img(TEXT("T_UI_SlotGlow"), FVector2D(200.f, 200.f) * K);
		Place(Glow, FVector2D::ZeroVector, FVector2D(200.f, 200.f) * K);
		Ring = Img(TEXT("T_UI_SlotRing"), FVector2D(200.f, 200.f) * K);
		Place(Ring, FVector2D::ZeroVector, FVector2D(200.f, 200.f) * K);
		Contact = Img(TEXT("T_UI_ContactShadow"), FVector2D(100.f, 50.f) * K);
		Place(Contact, FVector2D(2.f, 20.f) * K, FVector2D(100.f, 50.f) * K);
		ShardGlow = Img(TEXT("T_UI_ShardGlow"), FVector2D(200.f, 200.f) * K);
		IconShadow = Img(nullptr, FVector2D::ZeroVector);
		IconImage = Img(nullptr, FVector2D::ZeroVector);
		IconImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		IconShadow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UTextBlock* CoinLabel = nullptr;
		Coin = CountCoin(WidgetTree, &CoinLabel, 17.f * FMath::Max(K, 0.9f));
		CoinText = CoinLabel;
		Coin->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Coin))
		{
			S->SetAutoSize(true);
			S->SetAlignment(FVector2D(23.f / 48.f, 22.f / 48.f));
			S->SetPosition(FVector2D(2.f * Radius + 38.f * K, 2.f * Radius + 36.f * K));
		}
		// The pocket itself is the button (hover and click), round as the dish.
		Button = WidgetTree->ConstructWidget<USaltMenuButton>();
		FButtonStyle Style;
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None);
		Button->SetStyle(Style);
		Button->OnClicked.AddDynamic(this, &USaltItemSlotWidget::HandleClicked);
		Button->OnHovered.AddDynamic(this, &USaltItemSlotWidget::HandleHovered);
		Root->AddChildToCanvas(Button);
		Place(Button, FVector2D::ZeroVector, FVector2D(2.f * Radius, 2.f * Radius));
		Show(nullptr, 0, false);
	}
	return Super::RebuildWidget();
}

void USaltItemSlotWidget::Show(const FSaltItemDef* Item, int32 Count, bool bInSelected)
{
	if (!IconImage)
	{
		return;
	}
	bSelected = bInSelected;
	bHasItem = Item != nullptr;
	bAncient = Item && Item->Id == TEXT("AncientShard");
	if (UTexture2D* Icon = ItemIcon(Item))
	{
		IconImage->SetBrush(ImageBrush(Icon, FVector2D(96.f, 96.f)));
		UTexture2D* Shadow = ItemIconShadow(Item);
		IconShadow->SetBrush(ImageBrush(Shadow, FVector2D(96.f, 96.f)));
		IconShadow->SetVisibility(Shadow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		Contact->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
		IconShadow->SetVisibility(ESlateVisibility::Collapsed);
		Contact->SetVisibility(ESlateVisibility::Collapsed);
	}
	ShardGlow->SetVisibility(bAncient ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetCoinCount(Coin, CoinText, Item ? Count : 0);
}

void USaltItemSlotWidget::Turn()
{
	TurnTime = 0.f;
}

void USaltItemSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IconImage)
	{
		return;
	}
	Clock += InDeltaTime;
	// Selection animates in about 0.12 s: the glow and the gold ring come up, the item grows and lifts.
	SelAlpha = FMath::FInterpConstantTo(SelAlpha, bSelected ? 1.f : 0.f, InDeltaTime, 1.f / 0.12f);
	const float K = Radius / 50.f;
	const float E = FMath::InterpEaseInOut(0.f, 1.f, SelAlpha, 2.f);
	Glow->SetRenderOpacity(E);
	Ring->SetRenderOpacity(E);
	const float Size = FMath::Lerp(86.f, 96.f, E) * K;
	const float Lift = 14.f * K * E;
	const FVector2D IconCentre(0.f, -2.f * K - Lift);
	Place(IconImage, IconCentre, FVector2D(Size, Size));
	Place(IconShadow, IconCentre + FVector2D(3.f, 6.f + 12.f * E) * K, FVector2D(Size, Size));
	IconShadow->SetRenderOpacity(FMath::Lerp(1.f, 0.8f, E));
	// The shard's own slow turquoise glow (about 2.5 s).
	if (bAncient)
	{
		const float G = Size / 96.f * 200.f;
		Place(ShardGlow, IconCentre, FVector2D(G, G));
		ShardGlow->SetRenderOpacity(0.7f + 0.3f * FMath::Sin(Clock * TWO_PI / 2.5f));
	}
	// Turning it in the hand: the icon turns once about its upright axis (its width goes through zero) in 0.9 s.
	if (TurnTime >= 0.f)
	{
		TurnTime += InDeltaTime;
		const float T = FMath::Clamp(TurnTime / 0.9f, 0.f, 1.f);
		const float Sx = FMath::Cos(T * TWO_PI);
		IconImage->SetRenderScale(FVector2D(FMath::Abs(Sx) < 0.05f ? 0.05f : Sx, 1.f));
		IconShadow->SetRenderScale(FVector2D(FMath::Abs(Sx) < 0.05f ? 0.05f : Sx, 1.f));
		if (T >= 1.f)
		{
			TurnTime = -1.f;
			IconImage->SetRenderScale(FVector2D(1.f, 1.f));
			IconShadow->SetRenderScale(FVector2D(1.f, 1.f));
		}
	}
}

void USaltItemSlotWidget::HandleClicked()
{
	if (OnClickedFn)
	{
		OnClickedFn(Index);
	}
}

void USaltItemSlotWidget::HandleHovered()
{
	if (OnHoveredFn)
	{
		OnHoveredFn(Index);
	}
}

// --- Satchel ------------------------------------------------------------------------------------------------------

USaltSatchelWidget::USaltSatchelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> USaltSatchelWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = BuildSatchelScreen();
	}
	return Super::RebuildWidget();
}

UWidget* USaltSatchelWidget::BuildSatchelScreen()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	auto FullScreen = [Canvas](UWidget* W)
	{
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	};
	// The world behind: blurred, darkened, a vignette.
	UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
	Blur->SetBlurStrength(8.f);
	FullScreen(Blur);
	UImage* Dark = WidgetTree->ConstructWidget<UImage>();
	Dark->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	FullScreen(Dark);
	UImage* Vignette = WidgetTree->ConstructWidget<UImage>();
	Vignette->SetBrush(ImageBrush(Tex(TEXT("T_UI_Vignette")), FVector2D(256.f, 144.f)));
	FullScreen(Vignette);

	// The bag (everything that does not change), then the count, the pockets and the tag.
	UImage* Bag = WidgetTree->ConstructWidget<UImage>();
	Bag->SetBrush(ImageBrush(Tex(TEXT("T_UI_SatchelBag")), FVector2D(1520.f, 900.f)));
	At(Canvas, Bag, FVector2D(200.f, 180.f), FVector2D(1520.f, 900.f));
	// "7 / 24" debossed, right-aligned at x 1328 on the baseline 490 (IM Fell English 38 px: ascent 0.905 em).
	UTextBlock* CountInk = nullptr;
	UTextBlock* CountHighlight = nullptr;
	UOverlay* Count = Deboss(WidgetTree, FText::GetEmpty(), ESaltFace::Fell, 38.f, 0.f, ETextJustify::Right, &CountInk, &CountHighlight);
	CountDark = CountInk;
	CountLip = CountHighlight;
	At(Canvas, Count, FVector2D(1328.f, 490.f - 0.905f * 38.f), FVector2D::ZeroVector, FVector2D(1.f, 0.f));

	SlotWidgets.Reset();
	for (int32 I = 0; I < USaltInventoryComponent::Capacity; ++I)
	{
		USaltItemSlotWidget* W = CreateWidget<USaltItemSlotWidget>(this, USaltItemSlotWidget::StaticClass());
		W->Setup(I, [this](int32 SlotIndex) { SelectSlot(SlotIndex); }, [this](int32 SlotIndex) { SelectSlot(SlotIndex); }, 50.f);
		SlotWidgets.Add(W);
		At(Canvas, W, PocketCentre(I), FVector2D(200.f, 200.f), FVector2D(0.5f, 0.5f));
	}

	UWidget* Tag = BuildTag();
	At(Canvas, Tag, TagHole - TagHoleInFrame, FVector2D(400.f, 600.f));
	UImage* Twine = WidgetTree->ConstructWidget<UImage>();
	Twine->SetBrush(ImageBrush(Tex(TEXT("T_UI_TagTwineFront")), FVector2D(80.f, 80.f)));
	Twine->SetVisibility(ESlateVisibility::HitTestInvisible);
	At(Canvas, Twine, TagHole - FVector2D(40.f, 52.f), FVector2D(80.f, 80.f));
	return Canvas;
}

UWidget* USaltSatchelWidget::BuildTag()
{
	// The tag pivots at its hole; everything on it is laid out in its own 400 x 600 frame (hole at 190, 80).
	TagRoot = WidgetTree->ConstructWidget<UCanvasPanel>();
	TagRoot->SetRenderTransformPivot(FVector2D(TagHoleInFrame.X / 400.f, TagHoleInFrame.Y / 600.f));
	TagRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	UImage* Paper = WidgetTree->ConstructWidget<UImage>();
	Paper->SetBrush(ImageBrush(Tex(TEXT("T_UI_Tag")), FVector2D(400.f, 600.f)));
	if (UCanvasPanelSlot* S = TagRoot->AddChildToCanvas(Paper))
	{
		S->SetSize(FVector2D(400.f, 600.f));
	}
	const float L = TagHoleInFrame.X - 150.f + 28.f;   // 28 px inside the tag's left edge
	auto Put = [this, L](UWidget* W, float Top, float Width = 0.f)
	{
		UCanvasPanelSlot* S = TagRoot->AddChildToCanvas(W);
		S->SetPosition(FVector2D(L, Top));
		if (Width > 0.f)
		{
			S->SetSize(FVector2D(Width, 300.f));
			S->SetAutoSize(false);
		}
		else
		{
			S->SetAutoSize(true);
		}
		return S;
	};
	// Name (IM Fell English 44, baseline 60 under the hole), the count line (italic 21, baseline +34), an ink rule (+22).
	TagName = InkText(WidgetTree, FText::GetEmpty(), ESaltFace::Fell, 44.f);
	Put(TagName, TagHoleInFrame.Y + 60.f - 0.905f * 44.f);
	TagCount = FaceText(WidgetTree, FText::GetEmpty(), ESaltFace::FellItalic, 21.f, SRGB(90, 62, 36, 0.95f));
	Put(TagCount, TagHoleInFrame.Y + 94.f - 0.905f * 21.f);
	UImage* Rule = WidgetTree->ConstructWidget<UImage>();
	Rule->SetColorAndOpacity(SRGB(58, 38, 22, 0.5f));
	if (UCanvasPanelSlot* S = TagRoot->AddChildToCanvas(Rule))
	{
		S->SetPosition(FVector2D(L, TagHoleInFrame.Y + 116.f));
		S->SetSize(FVector2D(244.f, 1.2f));
	}
	// The description (Alegreya 21 on 29 px lines, wrapped to 244 px) and the actions below it, 46 px apart.
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UCanvasPanelSlot* S = TagRoot->AddChildToCanvas(Column))
	{
		S->SetPosition(FVector2D(L, TagHoleInFrame.Y + 152.f - 1.123f * 21.f));
		S->SetSize(FVector2D(250.f, 330.f));
	}
	TagDescription = InkText(WidgetTree, FText::GetEmpty(), ESaltFace::Alegreya, 21.f);
	TagDescription->SetAutoWrapText(true);
	TagDescription->SetWrapTextAt(244.f);
	TagDescription->SetLineHeightPercentage(29.f / (1.468f * 21.f));
	Column->AddChildToVerticalBox(TagDescription);
	TagActions = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(TagActions)->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
	auto Action = [this](const TCHAR* Key, const FText& Label)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChildToHorizontalBox(Keycap(WidgetTree, FText::FromString(Key), 30.f))->SetVerticalAlignment(VAlign_Center);
		UTextBlock* T = InkText(WidgetTree, Label, ESaltFace::Alegreya, 22.f);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(T))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(8.f, 0.f, 0.f, 4.f));
		}
		TagActions->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f));
		return T;
	};
	UseText = Action(TEXT("E"), FText::GetEmpty());
	Action(TEXT("R"), NSLOCTEXT("Saltbound", "TagTurn", "Turn it in your hand"));
	Action(TEXT("G"), NSLOCTEXT("Saltbound", "TagDrop", "Drop it"));
	return TagRoot;
}

void USaltSatchelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (USaltInventoryComponent* Satchel = Player->GetInventory())
		{
			InventoryChanged = Satchel->OnChanged.AddUObject(this, &USaltSatchelWidget::Refresh);
		}
	}
	Refresh();
}

void USaltSatchelWidget::NativeDestruct()
{
	if (const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn()))
	{
		if (USaltInventoryComponent* Satchel = Player->GetInventory())
		{
			Satchel->OnChanged.Remove(InventoryChanged);
		}
	}
	Super::NativeDestruct();
}

void USaltSatchelWidget::SelectSlot(int32 Index)
{
	if (Index >= 0 && Index < USaltInventoryComponent::Capacity && Index != Selected)
	{
		Selected = Index;
		// The tag swings: a ~6 degree kick that settles in about 0.8 s.
		TagSwingRate += (Index % 2 ? -1.f : 1.f) * 70.f;
		Refresh();
	}
}

FText USaltSatchelWidget::GetSelectedName() const
{
	return TagName ? TagName->GetText() : FText::GetEmpty();
}

FText USaltSatchelWidget::GetTagLine(int32 Line) const
{
	const UTextBlock* T = Line == 0 ? TagName.Get() : Line == 1 ? TagCount.Get() : TagDescription.Get();
	return T ? T->GetText() : FText::GetEmpty();
}

void USaltSatchelWidget::Refresh()
{
	const ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	if (!Satchel || !TagName)
	{
		return;
	}
	const TArray<FSaltItemStack>& Slots = Satchel->GetSlots();
	int32 Used = 0;
	for (int32 I = 0; I < SlotWidgets.Num(); ++I)
	{
		const FSaltItemStack Stack = Slots.IsValidIndex(I) ? Slots[I] : FSaltItemStack();
		const FSaltItemDef* Def = Stack.IsEmpty() ? nullptr : SaltItems::Find(Stack.Id);
		Used += Def ? 1 : 0;
		SlotWidgets[I]->Show(Def, Def ? Stack.Count : 0, I == Selected);
	}
	const FText CountText = FText::Format(NSLOCTEXT("Saltbound", "SatchelUsed", "{0} / {1}"), FText::AsNumber(Used),
		FText::AsNumber(USaltInventoryComponent::Capacity));
	CountDark->SetText(CountText);
	CountLip->SetText(CountText);
	const FSaltItemStack Sel = Slots.IsValidIndex(Selected) ? Slots[Selected] : FSaltItemStack();
	const FSaltItemDef* Def = Sel.IsEmpty() ? nullptr : SaltItems::Find(Sel.Id);
	if (Def)
	{
		TagName->SetText(Def->Name);
		TagCount->SetText(FText::Format(NSLOCTEXT("Saltbound", "TagCount", "{0} in this pocket · {1} fit"), FText::AsNumber(Sel.Count),
			FText::AsNumber(Def->MaxStack)));
		TagDescription->SetText(Def->Description);
		// No use works yet: an item's use is shown greyed; one without a use says "(later)".
		UseText->SetText(Def->UseText.IsEmpty() ? NSLOCTEXT("Saltbound", "TagUseLater", "(later)") : Def->UseText);
		UseText->SetColorAndOpacity(FSlateColor(SRGB(0x3A, 0x26, 0x16, 0.45f)));
		TagActions->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		TagName->SetText(NSLOCTEXT("Saltbound", "EmptyPocket", "Empty pocket"));
		TagCount->SetText(FText::GetEmpty());
		TagDescription->SetText(NSLOCTEXT("Saltbound", "EmptyPocketHelp", "Things you find on beaches, in wrecks and among the ruins go here."));
		TagActions->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool USaltSatchelWidget::DropSelected()
{
	ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr;
	UWorld* World = GetWorld();
	if (!Satchel || !World || !Satchel->GetSlots().IsValidIndex(Selected) || Satchel->GetSlots()[Selected].IsEmpty())
	{
		return false;
	}
	const FName Id = Satchel->GetSlots()[Selected].Id;
	if (Satchel->Remove(Id, 1) != 1)
	{
		return false;
	}
	// It lands on whatever is in front of the player's feet.
	const FVector Ahead = Player->GetActorLocation() + Player->GetActorForwardVector() * 70.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SaltDrop), false, Player);
	FVector Spot = Ahead - FVector(0.f, 0.f, 80.f);
	if (World->LineTraceSingleByChannel(Hit, Ahead + FVector(0.f, 0.f, 50.f), Ahead - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
	{
		Spot = Hit.ImpactPoint;
	}
	ASaltPickup* Pickup = World->SpawnActorDeferred<ASaltPickup>(ASaltPickup::StaticClass(), FTransform(FRotator(0.f, Player->GetActorRotation().Yaw, 0.f), Spot));
	if (Pickup)
	{
		Pickup->ItemId = Id;
		Pickup->Count = 1;
		Pickup->FinishSpawning(FTransform(FRotator(0.f, Player->GetActorRotation().Yaw, 0.f), Spot));
		if (Hit.GetComponent())
		{
			Pickup->AttachToComponent(Hit.GetComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	return true;
}

void USaltSatchelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The tag swings on its twine: a damped spring (it settles in about 0.8 s).
	const float Dt = FMath::Min(InDeltaTime, 0.05f);
	const float Omega = 12.f;
	TagSwingRate += (-Omega * Omega * TagSwing - 2.f * 0.42f * Omega * TagSwingRate) * Dt;
	TagSwing += TagSwingRate * Dt;
	if (TagRoot)
	{
		TagRoot->SetRenderTransformAngle(TagRest + TagSwing);
	}
}

void USaltSatchelWidget::RequestClose()
{
	if (ASaltPlayerController* PC = Cast<ASaltPlayerController>(GetOwningPlayer()))
	{
		PC->SetSatchelOpen(false);
	}
}

FReply USaltSatchelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	auto Is = [&Key](std::initializer_list<FKey> Keys)
	{
		for (const FKey& K : Keys)
		{
			if (Key == K)
			{
				return true;
			}
		}
		return false;
	};
	if (Is({ EKeys::Escape, EKeys::F10, EKeys::Gamepad_FaceButton_Right, EKeys::I, EKeys::Tab, EKeys::Gamepad_Special_Left }))
	{
		RequestClose();
	}
	else if (Is({ EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left }))
	{
		SelectSlot(Selected - 1);
	}
	else if (Is({ EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right }))
	{
		SelectSlot(Selected + 1);
	}
	else if (Is({ EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up }))
	{
		SelectSlot(Selected - Columns);
	}
	else if (Is({ EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down }))
	{
		SelectSlot(Selected + Columns);
	}
	else if (Is({ EKeys::R, EKeys::Gamepad_FaceButton_Top }))
	{
		if (SlotWidgets.IsValidIndex(Selected))
		{
			SlotWidgets[Selected]->Turn();
		}
	}
	else if (Is({ EKeys::G, EKeys::Gamepad_FaceButton_Left }))
	{
		DropSelected();
	}
	else
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return FReply::Handled();
}
