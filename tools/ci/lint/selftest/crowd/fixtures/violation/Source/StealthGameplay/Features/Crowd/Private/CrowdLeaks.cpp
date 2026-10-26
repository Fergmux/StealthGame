#include "Server/Roster/StealthRosterStore.h"
#include "Perception/AIPerceptionComponent.h"
void A(AAIController* C, APawn* P) { C->MoveToLocation(FVector::ZeroVector); }
void B(APawn* P) { P->AddMovementInput(FVector::ForwardVector); P->SetActorRotation(FRotator::ZeroRotator); }
void D(ACharacter* P) { P->GetCharacterMovement()->Velocity = FVector::ZeroVector; P->LaunchCharacter(FVector::UpVector, false, false); }
void E(UPawnCommandComponent* C, FAttackPayload Payload) { C->Attack(Payload); }
void F(AAIController* C, AActor* A) { C->SetFocus(A); C->Possess(nullptr); }
void G(AController* C) { C->SetControlRotation(FRotator::ZeroRotator); }
#include "Server/Objectives/ObjectivesSiteAssignment.h"
void H(APawn* P, APlayerState* S) { bool b = P->IsPlayerControlled(); auto T = S->GetTeam(); FSiteAssignment A; }
