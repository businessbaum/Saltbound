// Saltbound - the start menu (D66).

#include "UI/SaltStartMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Core/SaltSaveSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/SaltPlayerController.h"

namespace
{
	// Menus.html #mmB: the lines from (6cqw, 20cqw), 1.25cqw apart.
	const FVector2D StartMenuAt(115.f, 384.f);
}

USaltStartMenuWidget::USaltStartMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	BackdropKind = EBackdrop::Title;
	bPausesGame = false;
}

void USaltStartMenuWidget::Build()
{
	// The title exactly where the launch intro leaves it.
	UImage* Title = WidgetTree->ConstructWidget<UImage>();
	Title->SetBrush(SaltUI::ImageBrush(SaltUI::Tex(TEXT("T_UI_MenuTitle")), FVector2D(TitleW, TitleH)));
	Title->SetVisibility(ESlateVisibility::HitTestInvisible);
	SaltUI::At(Contents, Title, FVector2D(TitleX, TitleY), FVector2D(TitleW, TitleH));

	AddItem(NSLOCTEXT("SaltStart", "Continue", "Continue"), StartMenuAt);
	AddItem(NSLOCTEXT("SaltStart", "NewVoyage", "New voyage"), StartMenuAt);
	AddItem(NSLOCTEXT("SaltStart", "Voyages", "Voyages"), StartMenuAt);
	AddItem(NSLOCTEXT("SaltStart", "Settings", "Settings"), StartMenuAt);
	AddItem(NSLOCTEXT("SaltStart", "Quit", "Quit"), StartMenuAt);

	PlaceHints({ { { TEXT("W"), TEXT("S") }, NSLOCTEXT("SaltStart", "HintChoose", "choose") }, { { TEXT("E") }, NSLOCTEXT("SaltStart", "HintSail", "sail") } },
		115.f, false);
}

void USaltStartMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Chosen = INDEX_NONE;
	Refresh();
}

void USaltStartMenuWidget::Refresh()
{
	if (Items.Num() == 0)
	{
		return;
	}
	const USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(this);
	const int32 Active = Saves ? Saves->GetActiveVoyage() : INDEX_NONE;
	const FSaltVoyageInfo Info = Saves && Active != INDEX_NONE ? Saves->GetVoyageInfo(Active) : FSaltVoyageInfo();
	USaltMenuItem* Continue = Items[FindItem(EItem::Continue)];
	if (Info.bExists)
	{
		Continue->SetVisibility(ESlateVisibility::HitTestInvisible);
		Continue->SetSub(FText::Format(NSLOCTEXT("SaltStart", "ContinueSub", "Voyage {0} · {1} · {2}"), SaltMenu::Roman(Active + 1), SaltMenu::VoyageLine(Info),
			SaltMenu::PlayTime(Info.PlayTime)));
	}
	else
	{
		Continue->SetVisibility(ESlateVisibility::Collapsed);
	}
	StackItems(StartMenuAt);
	// The first line that shows is chosen when the menu comes up (and when the chosen one went away).
	if (!Items.IsValidIndex(Chosen) || Items[Chosen]->GetVisibility() == ESlateVisibility::Collapsed)
	{
		ChooseItem(Info.bExists ? FindItem(EItem::Continue) : FindItem(EItem::NewVoyage), true);
	}
}

void USaltStartMenuWidget::ItemUsed(int32 Index)
{
	ASaltPlayerController* PC = GetSaltController();
	if (!PC)
	{
		return;
	}
	switch ((EItem)Index)
	{
	case EItem::Continue:
		PC->ContinueVoyage();
		break;
	case EItem::NewVoyage:
		PC->NewVoyage();
		break;
	case EItem::Voyages:
		PC->OpenVoyages();
		break;
	case EItem::Settings:
		PC->OpenSettings();
		break;
	case EItem::Quit:
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
		break;
	}
}
