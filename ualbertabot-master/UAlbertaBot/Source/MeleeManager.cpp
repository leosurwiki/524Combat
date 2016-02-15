#include "MeleeManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

bool PairEdge::operator <(const PairEdge& that) const
{
	return this->distance<that.distance;
}

MeleeManager::MeleeManager() 
{ 

}

void MeleeManager::executeMicro(const BWAPI::Unitset & targets)
{
	if (formSquad(targets, 32 * 6, 32 * 9, 90, 40)){
		formed = true;
	}
	else {
		formed = false;
	}
	assignTargetsOld(targets);
}
//get real priority
double MeleeManager::getRealPriority(BWAPI::Unit attacker, BWAPI::Unit target)
{
	int groundWeaponRange = attacker->getType().groundWeapon().maxRange();
	int distA2T = std::max(0, attacker->getDistance(target) - groundWeaponRange);
	double Health = (((double)target->getHitPoints() + target->getShields()));
	return getAttackPriority(attacker, target)*exp(-distA2T / 5) / (Health + 160);
}
//assign the right enemy
std::unordered_map<BWAPI::Unit, BWAPI::Unit> MeleeManager::assignEnemy(const BWAPI::Unitset &meleeUnits, const BWAPI::Unitset & meleeUnitTargets)
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
	edges.resize(edges.size() / 12);
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
			if (sum >= 6)
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
void MeleeManager::assignTargetsOld(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & meleeUnits = getUnits();

	// figure out targets
	BWAPI::Unitset meleeUnitTargets;
	for (auto & target : targets) 
	{
		// conditions for targeting
		if (!(target->getType().isFlyer()) && 
			!(target->isLifted()) &&
			!(target->getType() == BWAPI::UnitTypes::Zerg_Larva) && 
			!(target->getType() == BWAPI::UnitTypes::Zerg_Egg) &&
			!(target->getType() == BWAPI::UnitTypes::Buildings)&&
			(target->isTargetable())&&
			target->isVisible()) 
		{
			meleeUnitTargets.insert(target);
		}
	}
	std::unordered_map<BWAPI::Unit, BWAPI::Unit> attacker2target = assignEnemy(meleeUnits,meleeUnitTargets);
	// for each meleeUnit
	for (auto & meleeUnit : meleeUnits)
	{
		// if the order is to attack or defend
		if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend) 
        {
            // run away if we meet the retreat critereon
			if (meleeUnitShouldRetreat(meleeUnit, targets) && meleeUnit->isUnderAttack())
            {
                BWAPI::Position fleeTo(centerOfAttackers);
				fleeTo.x = (-fleeTo.x + meleeUnit->getPosition().x*19) / 20;
				fleeTo.y = (-fleeTo.y + meleeUnit->getPosition().y*19) / 20;
                Micro::SmartMove(meleeUnit, fleeTo);
            }
			// if there are targets
			else if (!meleeUnitTargets.empty())
			{
				BWAPI::Unit target;
				if (attacker2target.find(meleeUnit) != attacker2target.end())
				{
					target = attacker2target[meleeUnit];
				}
				else  target = getTarget(meleeUnit, meleeUnitTargets);
				Micro::SmartAttackUnit(meleeUnit, target);

			}
			// if there are no targets
			else
			{
				// if we're not near the order position
				if (meleeUnit->getDistance(order.getPosition()) > 100)
				{
					// move to it
					Micro::SmartMove(meleeUnit, order.getPosition());
				}
			}
		}
		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition().x, meleeUnit->getPosition().y, 
			meleeUnit->getTargetPosition().x, meleeUnit->getTargetPosition().y, Config::Debug::ColorLineTarget);
		}
	}
}

std::pair<BWAPI::Unit, BWAPI::Unit> MeleeManager::findClosestUnitPair(const BWAPI::Unitset & attackers, const BWAPI::Unitset & targets)
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

// get a target for the meleeUnit to attack
BWAPI::Unit MeleeManager::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	BWAPI::Unit bestTarget = nullptr;

	// for each target possiblity
	for (auto & unit : targets)
	{
		if (!bestTarget || getRealPriority(meleeUnit, unit) > getRealPriority(meleeUnit,bestTarget))
		{
			bestTarget = unit;
		}
	}

	return bestTarget;
}

	// get the attack priority of a type in relation to a zergling
int MeleeManager::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) 
{
	BWAPI::UnitType type = unit->getType();

    if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar 
        && unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret
        && (BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Protoss_Dark_Templar) == 0))
    {
        return 13;
    }

	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar && unit->getType().isWorker())
	{
		return 12;
	}

	// highest priority is something that can attack us or aid in combat
    if (type ==  BWAPI::UnitTypes::Terran_Bunker)
    {
        return 11;
    }
    else if (type == BWAPI::UnitTypes::Terran_Medic || 
		(type.groundWeapon() != BWAPI::WeaponTypes::None && !type.isWorker()) || 
		type ==  BWAPI::UnitTypes::Terran_Bunker ||
		type == BWAPI::UnitTypes::Protoss_High_Templar ||
		type == BWAPI::UnitTypes::Protoss_Reaver ||
		(type.isWorker() && unitNearChokepoint(unit))) 
	{
		return 10;
	} 
	// next priority is worker
	else if (type.isWorker()) 
	{
		return 9;
	}
    // next is special buildings
	else if (type == BWAPI::UnitTypes::Zerg_Spawning_Pool)
	{
		return 5;
	}
	// next is special buildings
	else if (type == BWAPI::UnitTypes::Protoss_Pylon)
	{
		return 5;
	}
	// next is buildings that cost gas
	else if (type.gasPrice() > 0)
	{
		return 4;
	}
	else if (type.mineralPrice() > 0)
	{
		return 3;
	}
	// then everything else
	else
	{
		return 1;
	}
}

BWAPI::Unit MeleeManager::closestMeleeUnit(BWAPI::Unit target, const BWAPI::Unitset & meleeUnitsToAssign)
{
	double minDistance = 0;
	BWAPI::Unit closest = nullptr;

	for (auto & meleeUnit : meleeUnitsToAssign)
	{
		double distance = meleeUnit->getDistance(target);
		if (!closest || distance < minDistance)
		{
			minDistance = distance;
			closest = meleeUnit;
		}
	}
	
	return closest;
}

bool MeleeManager::meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
    // terran don't regen so it doesn't make any sense to retreat
    if (meleeUnit->getType().getRace() == BWAPI::Races::Terran)
    {
        return false;
    }

    // we don't want to retreat the melee unit if its shields or hit points are above the threshold set in the config file
    // set those values to zero if you never want the unit to retreat from combat individually

    if (meleeUnit->getShields() > Config::Micro::RetreatMeleeUnitShields || meleeUnit->getHitPoints() > Config::Micro::RetreatMeleeUnitHP)
    {
        return false;
    }

    // if there is a ranged enemy unit within attack range of this melee unit then we shouldn't bother retreating since it could fire and kill it anyway
    for (auto & unit : targets)
    {
        int groundWeaponRange = unit->getType().groundWeapon().maxRange();
        if (groundWeaponRange >= 64 )
        {
			//the possibility of retreat from rangeunits is determined by their distance and weaponrange
			return rand() % 100<std::max(0,100-((int) unit->getDistance(meleeUnit) - groundWeaponRange));
        }
    }

    return true;
}


// still has bug in it somewhere, use Old version
void MeleeManager::assignTargetsNew(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & meleeUnits = getUnits();
	// figure out targets
	BWAPI::Unitset meleeUnitTargets;
	for (auto & target : targets) 
	{
		// conditions for targeting
		if (!(target->getType().isFlyer()) && 
			!(target->isLifted()) &&
			!(target->getType() == BWAPI::UnitTypes::Zerg_Larva) && 
			!(target->getType() == BWAPI::UnitTypes::Zerg_Egg) &&
			target->isVisible()) 
		{
			meleeUnitTargets.insert(target);
		}
	}

    BWAPI::Unitset meleeUnitsToAssign(meleeUnits);
    std::map<BWAPI::Unit, int> attackersAssigned;

    for (auto & unit : meleeUnitTargets)
    {
        attackersAssigned[unit] = 0;
    }

    int smallThreshold = BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg ? 3 : 1;
    int bigThreshold = BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg ? 12 : 3;

    // keep assigning targets while we have attackers and targets remaining
    while (!meleeUnitsToAssign.empty() && !meleeUnitTargets.empty())
    {
        auto attackerAssignment = findClosestUnitPair(meleeUnitsToAssign, meleeUnitTargets);
        BWAPI::Unit & attacker = attackerAssignment.first;
        BWAPI::Unit & target = attackerAssignment.second;

        UAB_ASSERT_WARNING(attacker, "We should have chosen an attacker!");

        if (!attacker)
        {
            break;
        }

        if (!target)
        {
            Micro::SmartMove(attacker, order.getPosition());
            continue;
        }

        Micro::SmartAttackUnit(attacker, target);

        // update the number of units assigned to attack the target we found
        int & assigned = attackersAssigned[attackerAssignment.second];
        assigned++;

        // if it's a small / fast unit and there's more than 2 things attacking it already, don't assign more
        if ((target->getType().isWorker() || target->getType() == BWAPI::UnitTypes::Zerg_Zergling) && (assigned >= smallThreshold))
        {
            meleeUnitTargets.erase(target);
        }
        // if it's a building and there's more than 10 things assigned to it already, don't assign more
        else if (assigned > bigThreshold)
        {
            meleeUnitTargets.erase(target);
        }

        meleeUnitsToAssign.erase(attacker);
    }

    // if there's no targets left, attack move to the order destination
    if (meleeUnitTargets.empty())
    {
        for (auto & unit : meleeUnitsToAssign)    
        {
			if (unit->getDistance(order.getPosition()) > 100)
			{
				// move to it
				Micro::SmartMove(unit, order.getPosition());
                BWAPI::Broodwar->drawLineMap(unit->getPosition(), order.getPosition(), BWAPI::Colors::Yellow);
			}
        }
    }
}
