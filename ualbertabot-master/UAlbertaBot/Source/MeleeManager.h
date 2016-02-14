#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class MicroManager;
struct PairEdge;
struct PairEdge
{
	BWAPI::Unit attacker;
	BWAPI::Unit target;
	double distance;
	bool operator < (const PairEdge& that) const;
};
class MeleeManager : public MicroManager
{

public:
	BWAPI::Position centerOfAttackers;
	MeleeManager();
	~MeleeManager() {}
	void executeMicro(const BWAPI::Unitset & targets);

	BWAPI::Unit chooseTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets, std::map<BWAPI::Unit, int> & numTargeting);
	BWAPI::Unit closestMeleeUnit(BWAPI::Unit target, const BWAPI::Unitset & meleeUnitToAssign);
	int getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit);
	BWAPI::Unit getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
    bool meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
    std::pair<BWAPI::Unit, BWAPI::Unit> findClosestUnitPair(const BWAPI::Unitset & attackers, const BWAPI::Unitset & targets);
	void assignTargetsNew(const BWAPI::Unitset & targets);
    void assignTargetsOld(const BWAPI::Unitset & targets);
	double getRealPriority(BWAPI::Unit attacker, BWAPI::Unit target);
	bool compareTwoTargets(BWAPI::Unit attacker, BWAPI::Unit target1, BWAPI::Unit target2);
	int myMicroConstruct(const BWAPI::Unitset & attackers, const BWAPI::Unitset & targets);
	bool formSquad(const BWAPI::Unitset & targets);

};
}