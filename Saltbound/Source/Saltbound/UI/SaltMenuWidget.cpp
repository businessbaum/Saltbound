// Saltbound - the pause menu (D66).

#include "UI/SaltMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Boat/SaltBoat.h"
#include "Boat/SaltHullShape.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/SaltPlaces.h"
#include "Core/SaltSaveSubsystem.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/SaltInventoryComponent.h"
#include "Player/SaltPlayerCharacter.h"
#include "Player/SaltPlayerController.h"
#include "UI/SaltPaintedText.h"
#include "World/SaltLightingRig.h"
#include "World/SaltWindSubsystem.h"

namespace
{
	// Menus.html "Pause": "Paused" at (6cqw, 7cqw), the lines from 11.5cqw; the card 29 x 40cqw, 6cqw from the right, 7cqw
	// down, turned 1.2 degrees, its text 2.4 / 2.6cqw in.
	const FVector2D PauseMenuAt(115.f, 221.f);
	const FVector2D PauseCardAt(1248.f, 134.f);
	const FVector2D PauseCardSize(557.f, 768.f);
	constexpr float PauseCardPadX = 50.f;
	constexpr float PauseCardPadY = 46.f;
	/** Return to the title asks first when this much was played since the last save (s). */
	constexpr double PauseAskAfter = 10.0;

	UTextBlock* SpacedInk(UWidgetTree* Tree, const FText& Text, ESaltFace Face, float Size, float Spacing, float Alpha = 0.94f)
	{
		UTextBlock* T = SaltUI::InkText(Tree, Text, Face, Size, ETextJustify::Left, Alpha);
		FSlateFontInfo F = T->GetFont();
		F.LetterSpacing = FMath::RoundToInt(Spacing * 1000.f);
		T->SetFont(F);
		return T;
	}

	FText WindWords(float SpeedMs)
	{
		// The Beaufort names a sailor would use.
		if (SpeedMs < 1.5f) { return NSLOCTEXT("SaltPause", "Calm", "calm"); }
		if (SpeedMs < 3.3f) { return NSLOCTEXT("SaltPause", "LightAir", "a light breeze"); }
		if (SpeedMs < 5.5f) { return NSLOCTEXT("SaltPause", "Gentle", "a gentle breeze"); }
		if (SpeedMs < 8.f) { return NSLOCTEXT("SaltPause", "Moderate", "a moderate breeze"); }
		if (SpeedMs < 10.8f) { return NSLOCTEXT("SaltPause", "Fresh", "a fresh breeze"); }
		if (SpeedMs < 13.9f) { return NSLOCTEXT("SaltPause", "Strong", "a strong breeze"); }
		return NSLOCTEXT("SaltPause", "Gale", "a gale");
	}

	FText TimeWords(FName Preset)
	{
		if (Preset == TEXT("Day")) { return NSLOCTEXT("SaltPause", "Day", "day, the sun high"); }
		if (Preset == TEXT("Evening")) { return NSLOCTEXT("SaltPause", "Evening", "evening, the sun low in the west"); }
		if (Preset == TEXT("Dusk")) { return NSLOCTEXT("SaltPause", "Dusk", "dusk, the light going"); }
		if (Preset == TEXT("Night")) { return NSLOCTEXT("SaltPause", "Night", "night"); }
		if (Preset == TEXT("Dawn")) { return NSLOCTEXT("SaltPause", "Dawn", "dawn"); }
		return Preset.IsNone() ? FText::FromString(TEXT("-")) : FText::FromString(Preset.ToString().ToLower());
	}

	FText HoldWords(const ASaltBoat* Boat)
	{
		switch (Boat->GetHold())
		{
		case ESaltBoatHold::Moored: return NSLOCTEXT("SaltPause", "Moored", "moored");
		case ESaltBoatHold::Anchored: return NSLOCTEXT("SaltPause", "Anchored", "at anchor");
		default: return Boat->IsSailSet() ? NSLOCTEXT("SaltPause", "UnderSail", "under sail") : NSLOCTEXT("SaltPause", "Adrift", "adrift");
		}
	}
}

USaltMenuWidget::USaltMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
	BackdropKind = EBackdrop::Blurred;
	bShadeLeft = true;
}

void USaltMenuWidget::Build()
{
	SaltUI::At(Contents, SaltMenu::Paint(WidgetTree, NSLOCTEXT("SaltPause", "Paused", "Paused"), ESaltFace::FellSC, 25.f, ETextJustify::Left, 8.f),
		FVector2D(115.f, 134.f + 12.5f), FVector2D::ZeroVector, FVector2D(0.f, 0.5f));

	AddItem(NSLOCTEXT("SaltPause", "Resume", "Resume"), PauseMenuAt);
	AddItem(NSLOCTEXT("SaltPause", "SaveNow", "Save now"), PauseMenuAt);
	AddItem(NSLOCTEXT("SaltPause", "Voyages", "Voyages"), PauseMenuAt);
	AddItem(NSLOCTEXT("SaltPause", "Settings", "Settings"), PauseMenuAt);
	AddItem(NSLOCTEXT("SaltPause", "Title", "Return to the title"), PauseMenuAt);
	AddItem(NSLOCTEXT("SaltPause", "Quit", "Quit to desktop"), PauseMenuAt);

	// The card: torn parchment, turned a little, its lines inked.
	UCanvasPanel* Card = WidgetTree->ConstructWidget<UCanvasPanel>();
	SaltUI::At(Contents, Card, PauseCardAt, PauseCardSize);
	Card->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Card->SetRenderTransformAngle(1.2f);
	UImage* Paper = WidgetTree->ConstructWidget<UImage>();
	Paper->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_MenuCard")), FVector2D(620.f, 830.f)));
	if (UCanvasPanelSlot* S = Card->AddChildToCanvas(Paper))
	{
		S->SetPosition(FVector2D(-30.f, -30.f));
		S->SetSize(FVector2D(PauseCardSize.X + 60.f, PauseCardSize.Y + 60.f));
	}
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UCanvasPanelSlot* S = Card->AddChildToCanvas(Box))
	{
		S->SetPosition(FVector2D(PauseCardPadX, PauseCardPadY));
		S->SetSize(FVector2D(PauseCardSize.X - 2.f * PauseCardPadX, PauseCardSize.Y - 2.f * PauseCardPadY));
	}
	Box->AddChildToVerticalBox(SpacedInk(WidgetTree, NSLOCTEXT("SaltPause", "WhereYouAre", "Where you are"), ESaltFace::FellSC, 19.f, 0.25f, 0.66f));
	PlaceText = SaltUI::InkText(WidgetTree, FText::GetEmpty(), ESaltFace::FellSC, 50.f);
	Box->AddChildToVerticalBox(PlaceText)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	WhereText = SaltUI::InkText(WidgetTree, FText::GetEmpty(), ESaltFace::FellItalic, 23.f);
	Box->AddChildToVerticalBox(WhereText)->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
	UImage* Rule = WidgetTree->ConstructWidget<UImage>();
	FSlateBrush Line;
	Line.DrawAs = ESlateBrushDrawType::Image;
	Line.ImageSize = FVector2D(2.f, 2.f);
	Rule->SetBrush(Line);
	Rule->SetColorAndOpacity(SaltUI::SRGB(58, 38, 22, 0.45f));
	Box->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.f, 18.f, 0.f, 8.f));
	CardLine(Box, NSLOCTEXT("SaltPause", "Time", "Time"), TimeText, 14.f);
	CardLine(Box, NSLOCTEXT("SaltPause", "Wind", "Wind"), WindText, 14.f);
	CardLine(Box, NSLOCTEXT("SaltPause", "YourBoat", "Your boat"), BoatText, 14.f);
	CardLine(Box, NSLOCTEXT("SaltPause", "Satchel", "Satchel"), SatchelText, 14.f);
	CardLine(Box, NSLOCTEXT("SaltPause", "LastSaved", "Last saved"), SavedText, 14.f);

	PlaceHints({ { { TEXT("Esc") }, NSLOCTEXT("SaltPause", "HintResume", "resume") }, { { TEXT("E") }, NSLOCTEXT("SaltPause", "HintChoose", "choose") } },
		115.f, false);
}

UTextBlock* USaltMenuWidget::CardLine(UVerticalBox* Box, const FText& Label, TObjectPtr<UTextBlock>& OutValue, float Top)
{
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
	UTextBlock* L = SaltUI::InkText(WidgetTree, FText::Format(NSLOCTEXT("SaltPause", "CardLabel", "{0} · "), Label), ESaltFace::FellSC, 23.f);
	Line->AddChildToHorizontalBox(L)->SetVerticalAlignment(VAlign_Top);
	OutValue = SaltUI::InkText(WidgetTree, FText::GetEmpty(), ESaltFace::Alegreya, 23.f);
	OutValue->SetAutoWrapText(true);
	UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(OutValue);
	S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	S->SetVerticalAlignment(VAlign_Top);
	return OutValue;
}

void USaltMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ChooseItem(0, true);
	Refresh();
}

void USaltMenuWidget::Refresh()
{
	if (!PlaceText)
	{
		return;
	}
	UWorld* World = GetWorld();
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	ASaltPlayerCharacter* Player = Cast<ASaltPlayerCharacter>(GetOwningPlayerPawn());
	ASaltBoat* Boat = nullptr;
	for (TActorIterator<ASaltBoat> It(World); It; ++It)
	{
		Boat = *It;
		break;
	}

	// Where: the nearest named place, and whether aboard, ashore or in the water.
	const FVector Feet = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	const FSaltPlace* Place = SaltMenu::PlaceNear(Feet);
	PlaceText->SetText(Place ? Place->Name : NSLOCTEXT("SaltPause", "OpenSea", "The open sea"));
	bool bAboard = false;
	if (Boat && Player)
	{
		const FVector Local = Boat->GetActorTransform().InverseTransformPosition(Feet) * 0.01;
		bAboard = FMath::Abs(Local.X) < SaltHull::HalfLength && FMath::Abs(Local.Y) < SaltHull::HalfBeamAtSheer(Local.X) && Local.Z > -0.5 && Local.Z < 3.5;
	}
	WhereText->SetText(Player && Player->IsSwimming() ? NSLOCTEXT("SaltPause", "InWater", "in the water")
		: bAboard ? NSLOCTEXT("SaltPause", "Aboard", "aboard your boat") : NSLOCTEXT("SaltPause", "Ashore", "ashore"));

	const ASaltLightingRig* Rig = nullptr;
	for (TActorIterator<ASaltLightingRig> It(World); It; ++It)
	{
		Rig = *It;
		break;
	}
	TimeText->SetText(TimeWords(Rig ? Rig->CurrentPreset : NAME_None));

	if (const USaltWindSubsystem* Wind = World ? World->GetSubsystem<USaltWindSubsystem>() : nullptr)
	{
		const float Ms = Wind->GetSpeed() * 0.01f;
		WindText->SetText(Ms < 1.5f ? WindWords(Ms) : FText::Format(NSLOCTEXT("SaltPause", "WindFrom", "{0} from the {1}"), WindWords(Ms),
			FText::FromString(SaltMenu::CompassPoint(Wind->GetFromDirectionDeg()))));
	}

	if (!Boat)
	{
		BoatText->SetText(NSLOCTEXT("SaltPause", "NoBoat", "nowhere to be seen"));
	}
	else if (bAboard)
	{
		BoatText->SetText(FText::Format(NSLOCTEXT("SaltPause", "BoatAboard", "under your feet, {0}"), HoldWords(Boat)));
	}
	else
	{
		// North is +X, east +Y: the bearing from the player to the boat, clockwise from north.
		const FVector D = Boat->GetActorLocation() - Feet;
		const float Metres = D.Size2D() * 0.01f;
		const FString Distance = Metres < 1000.f ? FString::Printf(TEXT("%d m"), FMath::Max(FMath::RoundToInt(Metres), 1))
			: FString::Printf(TEXT("%.1f km"), Metres / 1000.f);
		BoatText->SetText(FText::Format(NSLOCTEXT("SaltPause", "BoatAway", "{0}, {1} {2}"), HoldWords(Boat), FText::FromString(Distance),
			FText::FromString(SaltMenu::CompassPoint(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X))))));
	}

	if (const USaltInventoryComponent* Satchel = Player ? Player->GetInventory() : nullptr)
	{
		int32 Used = 0;
		for (const FSaltItemStack& S : Satchel->GetSlots())
		{
			Used += S.IsEmpty() ? 0 : 1;
		}
		SatchelText->SetText(FText::Format(NSLOCTEXT("SaltPause", "Pockets", "{0} of {1} pockets"), FText::AsNumber(Used), FText::AsNumber(Satchel->GetCapacity())));
	}

	const bool bSaved = Saves && Saves->HasSave();
	SavedText->SetText(bSaved ? SaltMenu::Ago(Saves->GetSaveTime()) : NSLOCTEXT("SaltPause", "NotSaved", "not yet on this voyage"));
	if (Items.IsValidIndex(FindItem(EItem::Save)))
	{
		Items[FindItem(EItem::Save)]->SetSub(bSaved
			? FText::Format(NSLOCTEXT("SaltPause", "SaveSub", "Last saved {0}"), SaltMenu::Ago(Saves->GetSaveTime()))
			: NSLOCTEXT("SaltPause", "SaveSubNone", "Not saved on this voyage yet"));
	}
	StackItems(PauseMenuAt);
}

FString USaltMenuWidget::DescribeCard() const
{
	auto T = [](const UTextBlock* B) { return B ? B->GetText().ToString() : FString(); };
	return FString::Printf(TEXT("place='%s' where='%s' time='%s' wind='%s' boat='%s' satchel='%s' saved='%s'"), *T(PlaceText), *T(WhereText), *T(TimeText),
		*T(WindText), *T(BoatText), *T(SatchelText), *T(SavedText));
}

void USaltMenuWidget::Back()
{
	if (ASaltPlayerController* PC = GetSaltController())
	{
		PC->SetMenuOpen(false);
	}
}

void USaltMenuWidget::ItemUsed(int32 Index)
{
	ASaltPlayerController* PC = GetSaltController();
	USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	if (!PC)
	{
		return;
	}
	switch ((EItem)Index)
	{
	case EItem::Resume:
		PC->SetMenuOpen(false);
		break;
	case EItem::Save:
		if (Saves)
		{
			Saves->SaveGame(GetWorld(), TEXT("menu"));
			Refresh();
		}
		break;
	case EItem::Voyages:
		PC->OpenVoyages();
		break;
	case EItem::Settings:
		PC->OpenSettings();
		break;
	case EItem::Title:
		if (Saves && Saves->GetUnsavedTime() > PauseAskAfter)
		{
			PC->Ask(NSLOCTEXT("SaltPause", "AskTitle", "Return to the title?"),
				Saves->HasSave()
					? FText::Format(NSLOCTEXT("SaltPause", "AskTitleBody", "You last saved {0}. What happened since then stays at sea."), SaltMenu::Ago(Saves->GetSaveTime()))
					: NSLOCTEXT("SaltPause", "AskTitleBodyNone", "This voyage has not been saved yet. What happened stays at sea."),
				NSLOCTEXT("SaltPause", "AskTitleYes", "return"), NSLOCTEXT("SaltPause", "AskTitleNo", "stay aboard"),
				[PC]() { PC->ReturnToTitle(); });
		}
		else
		{
			PC->ReturnToTitle();
		}
		break;
	case EItem::Quit:
		if (Saves)
		{
			Saves->Autosave(GetWorld(), TEXT("quit"));
		}
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
		break;
	}
}
