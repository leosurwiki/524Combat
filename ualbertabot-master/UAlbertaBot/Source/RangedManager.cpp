#include "RangedManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

RangedManager::RangedManager()
{
}

void RangedManager::executeMicro(const BWAPI::Unitset & targets)
{
	if (formSquad(targets, 32 * 10, 32 * 9, 90, 40)){
		formed = true;
	}
	else {
		formed = false;
	}
	assignTargetsOld(targets);
}
//get real priority
double RangedManager::getRealPriority(BWAPI::Unit attacker, BWAPI::Unit target)
{
	int groundWeaponRange = attacker->getType().groundWeapon().maxRange();
	int distA2T = std::max(0, attacker->getDistance(target) - groundWeaponRange);
	double Health = (((double)target->getHitPoints() + target->getShields()));
	return getAttackPriority(attacker, target)*exp(-distA2T / 5) / (Health + 160);
}
//assign the right enemy
std::unordered_map<BWAPI::Unit, BWAPI::Unit> RangedManager::assignEnemy(const BWAPI::Unitset &meleeUnits, const BWAPI::Unitset & meleeUnitTargets)
{
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> attacker2target;
	std::vector<PairEdge> edges(meleeUnits.size()*meleeUnitTargets.size());
	int top = 0;
	centerOfAttackers.x = 0;
	centerOfAttackers.y = 0;
	for (auto &attacker : meleeUnits)
	{
		centerOfAttackers.x += attacker->getPosition().x;
		centerOfAttackers.y += attacker->getPosition().y;
		for (auto &target : meleeUnitTargets)
		{
			edges[top].attacker = attacker;
			edges[top].target = target;
			int groundWeaponRange = attacker->getType().groundWeapon().maxRange();
			edges[top++].distance = -getRealPriority(attacker, target);
		}
	}
	centerOfAttackers.x /= meleeUnits.size();
	centerOfAttackers.y /= meleeUnits.size();
	BWAPI::Broodwar->drawCircleMap(centerOfAttackers, 20, BWAPI::Colors::Brown, true);
	sort(edges.begin(), edges.end());
	edges.resize(edges.size()/2);
	sort(edges.begin(), edges.end(), [](PairEdge a, PairEdge b)
	{
		return (int)a.target < (int)b.target;
	});
	PairEdge dummy;
	dummy.target = nullptr;
	edges.push_back(dummy);
	BWAPI::Unit p = nullptr;
	int sum = 0;
	for (auto idx = edges.begin(); idx->target != nullptr; idx++)
	{
		if (p != idx->target)
		{
			sum = 0;
			p = idx->target;
		}
		else
		{
			sum++;
			//assign at most 7 units attack
			BWAPI::Broodwar <<( idx->target->getHitPoints()+idx->target->getShields()) / idx->attacker->getType().groundWeapon().damageAmount() << std::endl;
			if (sum<std::min(6,std::max(idx->target->getHitPoints()/idx->attacker->getType().groundWeapon().damageAmount()-1,1)))
			{
				idx->attacker = nullptr;
			}
		}
	}
	for (bool halt = true; halt == false; halt = true)
	{
		auto tmpRangeStart = edges.begin();
		auto maxRangeStart = tmpRangeStart, maxRangeEnd = tmpRangeStart;
		double tmpsum = 0, tmpres = INT_MIN;
		for (auto idx = edges.begin(); idx->target != nullptr; idx++)
		{
			if (attacker2target.find(idx->attacker) != attacker2target.end())
				continue;
			if (idx->target != (idx + 1)->target)
			{
				if (tmpsum > tmpres)
				{
					tmpres = tmpsum;
					maxRangeStart = tmpRangeStart;
					maxRangeEnd = idx + 1;
				}
				tmpsum = 0;
				tmpRangeStart = idx + 1;
			}
			else
				tmpsum += getRealPriority(idx->attacker, idx->target);
		}
		for (auto kdx = maxRangeStart; kdx != maxRangeEnd; kdx++)
		{
			if (attacker2target.find(kdx->attacker) != attacker2target.end())
				continue;
			attacker2target[kdx->attacker] = kdx->target;
			halt = false;
		}
	}
	return attacker2target;
}


void RangedManager::assignTargetsOld(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & rangedUnits = getUnits();

	// figure out targets
	BWAPI::Unitset rangedUnitTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(rangedUnitTargets, rangedUnitTargets.end()), [](BWAPI::Unit u){ return u->isVisible(); });
	auto attacker2target = assignEnemy(rangedUnits, rangedUnitTargets);
	for (auto & rangedUnit : rangedUnits)
	{
		// train sub units such as scarabs or interceptors
		//trainSubUnits(rangedUnit);

		// if the order is to attack or defend
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
		{
			// if there are targets
			if (!rangedUnitTargets.empty())
			{
				
				// find the best target for this dragoon
				auto targetIdx = attacker2target.find(rangedUnit);
				BWAPI::Unit target = targetIdx == attacker2target.end() ? getTarget(rangedUnit, rangedUnitTargets) : targetIdx->first;
				if (target && Config::Debug::DrawUnitTargetInfo)
				{
					BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), rangedUnit->getTargetPosition(), BWAPI::Colors::Purple);
				}


				// attack it
				if (Config::Micro::KiteWithRangedUnits)
				{
					if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || rangedUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
					{
						Micro::MutaDanceTarget(rangedUnit, target);
					}
					else
					{
						BWAPI::Broodwar->drawTextScreen(200, 350, "%s", "kite target");
						Micro::SmartKiteTarget(rangedUnit, target);
					}
				}
				else
				{
					Micro::SmartAttackUnit(rangedUnit, target);
				}
			}
			// if there are no targets
			else
			{
				// if we're not near the order position
				if (rangedUnit->getDistance(order.getPosition()) > 100)
				{
					// move to it
					Micro::SmartAttackMove(rangedUnit, order.getPosition());
				}
			}
		}
	}
}

std::pair<BWAPI::Unit, BWAPI::Unit> RangedManager::findClosestUnitPair(const BWAPI::Unitset & attackers, const BWAPI::Unitset & targets)
{
	std::pair<BWAPI::Unit, BWAPI::Unit> closestPair(nullptr, nullptr);
	double closestDistance = std::numeric_limits<double>::max();

	for (auto & attacker : attackers)
	{
		BWAPI::Unit target = getTarget(attacker, targets);
		double dist = attacker->getDistance(attacker);

		if (!closestPair.first || (dist < closestDistance))
		{
			closestPair.first = attacker;
			closestPair.second = target;
			closestDistance = dist;
		}
	}

	return closestPair;
}

// get a target for the zealot to attack
BWAPI::Unit RangedManager::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
	int bestPriorityDistance = 1000000;
	int bestPriority = 0;

	double bestLTD = 0;

	int highPriority = 0;
	double closestDist = std::numeric_limits<double>::infinity();
	BWAPI::Unit closestTarget = nullptr;
	
	for (const auto & target : targets)
	{
		double distance = rangedUnit->getDistance(target);
		double LTD = UnitUtil::CalculateLTD(target, rangedUnit);
		int priority = getAttackPriority(rangedUnit, target);
		bool targetIsThreat = LTD > 0;

		if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
		{
			closestDist = distance;
			highPriority = priority;
			closestTarget = target;
		}
	}

	return closestTarget;
}

// get the attack priority of a type in relation to a zergling
int RangedManager::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	BWAPI::UnitType rangedType = rangedUnit->getType();
	BWAPI::UnitType targetType = target->getType();


	if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Scourge)
	{
		if (target->getType() == BWAPI::UnitTypes::Protoss_Carrier)
		{

			return 100;
		}

		if (target->getType() == BWAPI::UnitTypes::Protoss_Corsair)
		{
			return 90;
		}
	}

	bool isThreat = rangedType.isFlyer() ? targetType.airWeapon() != BWAPI::WeaponTypes::None : targetType.groundWeapon() != BWAPI::WeaponTypes::None;

	if (target->getType().isWorker())
	{
		isThreat = false;
	}

	if (target->getType() == BWAPI::UnitTypes::Zerg_Larva || target->getType() == BWAPI::UnitTypes::Zerg_Egg)
	{
		return 0;
	}

	if (rangedUnit->isFlying() && target->getType() == BWAPI::UnitTypes::Protoss_Carrier)
	{
		return 101;
	}

	// if the target is building something near our base something is fishy
	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
	if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()) && target->getDistance(ourBasePosition) < 1200)
	{
		return 100;
	}

	if (target->getType().isBuilding() && (target->isCompleted() || target->isBeingConstructed()) && target->getDistance(ourBasePosition) < 1200)
	{
		return 90;
	}

	// highest priority is something that can attack us or aid in combat
	if (targetType == BWAPI::UnitTypes::Terran_Bunker || isThreat)
	{
		return 11;
	}
	// next priority is worker
	else if (targetType.isWorker())
	{
		if (rangedUnit->getType() == BWAPI::UnitTypes::Terran_Vulture)
		{
			return 11;
		}

		return 11;
	}
	// next is special buildings
	else if (targetType == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 5;
	}
	// next is special buildings
	else if (targetType == BWAPI::UnitTypes::Protoss_Pylon)
	{
		return 5;
	}
	// next is buildings that cost gas
	else if (targetType.gasPrice() > 0)
	{
		return 4;
	}
	else if (targetType.mineralPrice() > 0)
	{
		return 3;
	}
	// then everything else
	else
	{
		return 1;
	}
}

BWAPI::Unit RangedManager::closestrangedUnit(BWAPI::Unit target, std::set<BWAPI::Unit> & rangedUnitsToAssign)
{
	double minDistance = 0;
	BWAPI::Unit closest = nullptr;

	for (auto & rangedUnit : rangedUnitsToAssign)
	{
		double distance = rangedUnit->getDistance(target);
		if (!closest || distance < minDistance)
		{
			minDistance = distance;
			closest = rangedUnit;
		}
	}

	return closest;
}


// still has bug in it somewhere, use Old version
void RangedManager::assignTargetsNew(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & rangedUnits = getUnits();

	// figure out targets
	BWAPI::Unitset rangedUnitTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(rangedUnitTargets, rangedUnitTargets.end()), [](BWAPI::Unit u){ return u->isVisible(); });

	BWAPI::Unitset rangedUnitsToAssign(rangedUnits);
	std::map<BWAPI::Unit, int> attackersAssigned;

	for (auto & unit : rangedUnitTargets)
	{
		attackersAssigned[unit] = 0;
	}

	// keep assigning targets while we have attackers and targets remaining
	while (!rangedUnitsToAssign.empty() && !rangedUnitTargets.empty())
	{
		auto attackerAssignment = findClosestUnitPair(rangedUnitsToAssign, rangedUnitTargets);
		BWAPI::Unit & attacker = attackerAssignment.first;
		BWAPI::Unit & target = attackerAssignment.second;

		UAB_ASSERT_WARNING(attacker, "We should have chosen an attacker!");

		if (!attacker)
		{
			break;
		}

		if (!target)
		{
			Micro::SmartAttackMove(attacker, order.getPosition());
			continue;
		}

		if (Config::Micro::KiteWithRangedUnits)
		{
			if (attacker->getType() == BWAPI::UnitTypes::Zerg_Mutalisk || attacker->getType() == BWAPI::UnitTypes::Terran_Vulture)
			{
				Micro::MutaDanceTarget(attacker, target);
			}
			else
			{
				Micro::SmartKiteTarget(attacker, target);
			}
		}
		else
		{
			Micro::SmartAttackUnit(attacker, target);
		}

		// update the number of units assigned to attack the target we found
		int & assigned = attackersAssigned[attackerAssignment.second];
		assigned++;

		// if it's a small / fast unit and there's more than 2 things attacking it already, don't assign more
		if ((target->getType().isWorker() || target->getType() == BWAPI::UnitTypes::Zerg_Zergling) && (assigned > 2))
		{
			rangedUnitTargets.erase(target);
		}
		// if it's a building and there's more than 10 things assigned to it already, don't assign more
		else if (target->getType().isBuilding() && (assigned > 10))
		{
			rangedUnitTargets.erase(target);
		}

		rangedUnitsToAssign.erase(attacker);
	}

	// if there's no targets left, attack move to the order destination
	if (rangedUnitTargets.empty())
	{
		for (auto & unit : rangedUnitsToAssign)
		{
			if (unit->getDistance(order.getPosition()) > 100)
			{
				// move to it
				Micro::SmartAttackMove(unit, order.getPosition());
			}
		}
	}
}