// Saltbound - checks of people's lines (sb.Dev.RunTalkTests, FH2, D81): Lines.json reads without problems, the Keeper's first talk
// (the sheet, the words written out, the answers on their keys, the gift), his entries following the story, answers hidden by their
// conditions, walking away, the talk's keys above play's, the call in passing at casting off. It leaves the world in the prologue
// (the play session ends after it).

#include "Boat/SaltBoat.h"
#include "Boat/SaltHullDamage.h"
#include "Core/SaltSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SaltPlayerCharacter.h"
#include "Saltbound.h"
#include "UI/SaltTalkWidget.h"
#include "World/SaltDig.h"
#include "World/SaltPrologue.h"
#include "World/SaltTalk.h"

#if !UE_BUILD_SHIPPING
namespace
{
	FString Keys(const USaltTalkSubsystem* Talk)
	{
		FString Out;
		for (const FSaltShownChoice& C : Talk->GetShownChoices())
		{
			Out += C.Key;
		}
		return Out;
	}

	void RunTalkTests(const TArray<FString>& Args, UWorld* World)
	{
		int32 Passed = 0;
		int32 Failed = 0;
		auto Report = [&](const TCHAR* Name, bool bPass, const FString& Details)
		{
			(bPass ? Passed : Failed)++;
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST %s %s %s"), Name, bPass ? TEXT("PASS") : TEXT("FAIL"), *Details);
		};
		USaltTalkSubsystem* Talk = World ? World->GetSubsystem<USaltTalkSubsystem>() : nullptr;
		USaltPrologueSubsystem* Prologue = World ? World->GetSubsystem<USaltPrologueSubsystem>() : nullptr;
		USaltDigSubsystem* Digs = World ? World->GetSubsystem<USaltDigSubsystem>() : nullptr;
		USaltSaveSubsystem* Saves = USaltSaveSubsystem::Get(World);
		ASaltPlayerCharacter* Player = World ? Cast<ASaltPlayerCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		ASaltBoat* Boat = nullptr;
		for (TActorIterator<ASaltBoat> It(World); It; ++It)
		{
			Boat = *It;
			break;
		}
		USaltHullDamageComponent* Damage = Boat ? Boat->GetHullDamage() : nullptr;
		if (!Talk || !Prologue || !Digs || !Saves || !Player || !Damage)
		{
			UE_LOG(LogSaltbound, Error, TEXT("SB_TEST TalkSuite FAIL reason=missing_talk_prologue_save_player_or_boat"));
			UE_LOG(LogSaltbound, Display, TEXT("SB_TEST TalkSuite DONE passed=0 failed=1"));
			return;
		}
		Talk->End();

		// The lines read without problems: every entry and answer leads to a line, every action and condition is known.
		const TArray<FString> Problems = Talk->Load();
		const FSaltSpeaker* Keeper = Talk->FindSpeaker(TEXT("Keeper"));
		Report(TEXT("LinesLoad"), Problems.Num() == 0 && Keeper && Keeper->Entries.Num() >= 5 && Talk->GetLines().Num() >= 10
			&& Talk->GetCalls().Contains(TEXT("keeper.castoff")), FString::Printf(TEXT("problems=%d%s entries=%d lines=%d calls=%d"), Problems.Num(),
			Problems.Num() ? *(TEXT(" first='") + Problems[0] + TEXT("'")) : TEXT(""), Keeper ? Keeper->Entries.Num() : 0, Talk->GetLines().Num(),
			Talk->GetCalls().Num()));

		// The castaway's first talk: the Keeper's hand-over opens it; the sheet shows his name and the line written out word by word;
		// the answers (1 who, 2 the sail, E the plain one) come when the words are done.
		Prologue->Begin(false);
		Digs->HandleKeeper(nullptr, Player);
		USaltTalkWidget* W = Talk->GetWidget();
		const FString Full = W ? W->GetLineText() : FString();
		const bool bWritingAtFirst = W && W->IsWriting() && W->GetWrittenText().Len() < Full.Len();
		if (W)
		{
			W->FinishWriting();
		}
		Report(TEXT("KeeperFirstTalk"), Talk->IsTalking() && Talk->GetSpeakerId() == TEXT("Keeper") && Talk->GetLineId() == TEXT("keeper.first")
			&& W && W->IsShowingLine() && bWritingAtFirst && W->GetWrittenText() == Full && Full.Contains(TEXT("Cartographer"))
			&& Keys(Talk) == TEXT("12E") && W->GetChoiceCount() == 3 && !Player->IsToolOwned(SaltDig::ShovelTool), FString::Printf(
			TEXT("line=%s sheet=%d writing=%d written=%d/%d keys=%s rows=%d shovelYet=%d"), *Talk->GetLineId().ToString(), W && W->IsShowingLine() ? 1 : 0,
			bWritingAtFirst ? 1 : 0, W ? W->GetWrittenText().Len() : 0, Full.Len(), *Keys(Talk), W ? W->GetChoiceCount() : 0,
			Player->IsToolOwned(SaltDig::ShovelTool) ? 1 : 0));

		// The talk's keys are mapped above play's while he speaks.
		const bool bKeysWhileTalking = Talk->IsInputActive();

		// Answers by their keys: 1 (who was the Cartographer?), 1 (the sail), E (where?): the gift, then E ends it.
		Talk->SetInstant(true);
		TArray<FString> Path;
		Path.Add(Talk->GetLineId().ToString());
		Talk->Choose(0);
		Path.Add(Talk->GetLineId().ToString());
		Talk->Choose(0);
		Path.Add(Talk->GetLineId().ToString());
		Talk->Answer();
		Path.Add(Talk->GetLineId().ToString());
		const bool bGifts = Player->IsToolOwned(SaltDig::ShovelTool) && Player->IsToolOwned(TEXT("Lantern")) && Saves->HasBeat(SaltStory::Keeper);
		Talk->Answer();
		const FString Walked = FString::Join(Path, TEXT(">"));
		Report(TEXT("AnswersByKey"), Walked == TEXT("keeper.first>keeper.who>keeper.sail>keeper.gift") && bGifts && !Talk->IsTalking()
			&& bKeysWhileTalking && !Talk->IsInputActive(), FString::Printf(TEXT("path=%s gifts=%d keysWhileTalking=%d keysAfter=%d"), *Walked,
			bGifts ? 1 : 0, bKeysWhileTalking ? 1 : 0, Talk->IsInputActive() ? 1 : 0));

		// His entries follow the story: the cache, mending, ready to sail; casting off brings his call in passing.
		TArray<FString> Entries;
		auto EntryNow = [&]()
		{
			Talk->Begin(TEXT("Keeper"), Player);
			Entries.Add(Talk->GetLineId().ToString());
			Talk->End();
		};
		EntryNow();
		Saves->SetBeat(SaltStory::Cache, Prologue->Now());
		EntryNow();
		Saves->SetBeat(SaltStory::SailMended, Prologue->Now());
		EntryNow();
		Saves->SetBeat(SaltStory::CastOff, Prologue->Now());
		const bool bCall = W && W->IsShowingCall() && W->GetCallText().Contains(TEXT("reef"));
		EntryNow();
		const FString Story = FString::Join(Entries, TEXT(">"));
		Report(TEXT("EntriesFollowTheStory"), Story == TEXT("keeper.cache>keeper.mend>keeper.ready>keeper.idle") && bCall, FString::Printf(
			TEXT("entries=%s call=%d '%s'"), *Story, bCall ? 1 : 0, W ? *W->GetCallText() : TEXT("")));

		// An answer whose condition does not hold is not shown: with the sail whole, "Her sail is ripped" is gone.
		Saves->ClearBeats();
		const int32 OwnTear = Damage->GetSailTear();
		Damage->RestoreState({}, 0, 0);
		Talk->Begin(TEXT("Keeper"), Player);
		const FString WholeKeys = Keys(Talk);
		Talk->End();
		Damage->RestoreState({}, 0, OwnTear);
		Report(TEXT("ConditionsHideAnswers"), WholeKeys == TEXT("1E"), FString::Printf(TEXT("keys with the sail whole=%s (ripped: 12E)"), *WholeKeys));

		// Walking away ends the talk.
		Talk->Begin(TEXT("Keeper"), Player);
		const bool bOpen = Talk->IsTalking();
		const FVector Was = Player->GetActorLocation();
		Player->SetActorLocation(Was + FVector(500.0, 0.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
		Talk->Tick(0.1f);
		const bool bEnded = !Talk->IsTalking();
		Player->SetActorLocation(Was, false, nullptr, ETeleportType::TeleportPhysics);
		Report(TEXT("WalkingAwayEnds"), bOpen && bEnded && !Talk->IsInputActive(), FString::Printf(TEXT("open=%d endedAt5m=%d"), bOpen ? 1 : 0,
			bEnded ? 1 : 0));

		// Nobody by that name has nothing to say.
		Report(TEXT("UnknownSpeaker"), !Talk->Begin(TEXT("Nobody"), Player) && !Talk->IsTalking(), TEXT("begin=0"));

		Talk->SetInstant(false);
		Prologue->Begin(false);
		UE_LOG(LogSaltbound, Display, TEXT("SB_TEST TalkSuite DONE passed=%d failed=%d"), Passed, Failed);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRunTalkTests(
		TEXT("sb.Dev.RunTalkTests"),
		TEXT("FH2 (D81): checks people's lines (the file, the Keeper's talk, answers, entries by the story, conditions, walking away, calls)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunTalkTests));
}
#endif
