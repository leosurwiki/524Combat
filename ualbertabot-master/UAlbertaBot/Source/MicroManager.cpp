#include "MicroManager.h"

using namespace UAlbertaBot;

MicroManager::MicroManager() :_formation(true)
{
}

void MicroManager::setUnits(const BWAPI::Unitset & u) 
{ 
	_units = u; 
}

//set whether it needs to form
void MicroManager::setFormation(bool f)
{
	_formation = f;
}


BWAPI::Position MicroManager::calcCenter() const
{
    if (_units.empty())
    {
        if (Config::Debug::DrawSquadInfo)
        {
            BWAPI::Broodwar->printf("Squad::calcCenter() called on empty squad");
        }
        return BWAPI::Position(0,0);
    }

	BWAPI::Position accum(0,0);
	for (auto & unit : _units)
	{
		accum += unit->getPosition();
	}
	return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

void MicroManager::execute(const SquadOrder & inputOrder)
{
	// Nothing to do if we have no units
	if (_units.empty() || !(inputOrder.getType() == SquadOrderTypes::Attack || inputOrder.getType() == SquadOrderTypes::Defend))
	{
		return;
	}

	order = inputOrder;
	drawOrderText();

	// Discover enemies within region of interest
	BWAPI::Unitset nearbyEnemies;

	// if the order is to defend, we only care about units in the radius of the defense
	if (order.getType() == SquadOrderTypes::Defend)
	{
		MapGrid::Instance().GetUnits(nearbyEnemies, order.getPosition(), order.getRadius(), false, true);
	
	} // otherwise we want to see everything on the way
	else if (order.getType() == SquadOrderTypes::Attack) 
	{
		MapGrid::Instance().GetUnits(nearbyEnemies, order.getPosition(), order.getRadius(), false, true);
		for (auto & unit : _units) 
		{
			BWAPI::Unit u = unit;
			BWAPI::UnitType t = u->getType();
			MapGrid::Instance().GetUnits(nearbyEnemies, unit->getPosition(), order.getRadius(), false, true);
		}
	}

	// the following block of code attacks all units on the way to the order position
	// we want to do this if the order is attack, defend, or harass
	if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend) 
	{
        // if this is a worker defense force
        if (_units.size() == 1 && (*_units.begin())->getType().isWorker())
        {
            executeMicro(nearbyEnemies);
        }
        // otherwise it is a normal attack force
        else
        {
            // if this is a defense squad then we care about all units in the area
            if (order.getType() == SquadOrderTypes::Defend)
            {
                executeMicro(nearbyEnemies);
            }
            // otherwise we only care about workers if they are in their own region
            else
            {
                 // if this is the an attack squad
                BWAPI::Unitset workersRemoved;

                for (auto & enemyUnit : nearbyEnemies) 
		        {
                    // if its not a worker add it to the targets
			        if (!enemyUnit->getType().isWorker())
                    {
                        workersRemoved.insert(enemyUnit);
                    }
                    // if it is a worker
                    else
                    {
                        for (BWTA::Region * enemyRegion : InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->enemy()))
                        {
                            // only add it if it's in their region
                            if (BWTA::getRegion(BWAPI::TilePosition(enemyUnit->getPosition())) == enemyRegion)
                            {
                                workersRemoved.insert(enemyUnit);
                            }
                        }
                    }
		        }

		        // Allow micromanager to handle enemies
		        executeMicro(workersRemoved);
            }
        }
	}	
}

const BWAPI::Unitset & MicroManager::getUnits() const 
{ 
    return _units; 
}

bool MicroManager ::getFormation() {
	return _formation;
}

//dist is distance between middle unit and target gravity center, interval is length of arc between two units
bool MicroManager::formSquad(const BWAPI::Unitset & targets, int dist, int radius, double angle, int interval)
{
	const BWAPI::Unitset & meleeUnits = getUnits();
	bool attacked = false;
	BWAPI::Position tpos = targets.getPosition();
	BWAPI::Position mpos = meleeUnits.getPosition();

	//do not from squad when fighting or close to enemy
	for (auto & unit : meleeUnits){
		if (unit->isUnderAttack() || unit->isAttackFrame() || targets.size() == 0|| unit->getDistance(tpos) < 80)
			attacked = true;
	}
	if (attacked) {
		BWAPI::Broodwar->drawTextScreen(200, 340, "%s", "Attacked or No targets, Stop Formation");
		return false;
	}
	//if there is a building near the unit, do not form
	for (auto & target : targets){
		auto type = target->getType();
		if (type.isBuilding()){
			return false;
		}
	}
	//Formation is set false by Squad for 5 seconds after formation finished once
	if (!getFormation()){
		BWAPI::Broodwar->drawTextScreen(200, 340, "%s", "Finished Formation");
		return false;
	}
	//BWAPI::Broodwar->drawTextScreen(200, 340, "%s", "Forming");

	const double PI = 3.14159265;

	double ang = angle / 180 * PI;

	//the angle of mid_point on arc
	double m_ang = atan2(mpos.y - tpos.y, mpos.x - tpos.x);
	//circle center
	int cx = (int)(tpos.x - (radius-dist) * cos(m_ang));
	int cy = (int)(tpos.y - (radius-dist) * sin(m_ang));
	BWAPI::Position c;
	c.x = cx; c.y = cy;
	//mid_point on arc
	BWAPI::Position m;
	m.x = (int)(cx + radius*cos(m_ang));
	m.y = (int)(cy + radius*sin(m_ang));
	BWAPI::Broodwar->drawLineMap(c, m, BWAPI::Colors::Yellow);

	BWAPI::Unitset unassigned;
	for (auto & unit : meleeUnits){
		unassigned.insert(unit);
	}

	//move every positions on the arc to the closest unit
	BWAPI::Position tmp;
	int try_time = 0;
	int r = radius;
	int total_dest_dist = 0;
	int num_assigned = 0;

	while (unassigned.size() > 0 && try_time < 5){
		double ang_interval = interval * 1.0 / r;
		double final_ang;
		int num_to_assign;
		int max_units = (int)(ang / ang_interval) + 1;
		if (unassigned.size() < (unsigned)max_units){
			num_to_assign = unassigned.size();
			final_ang = ang_interval * num_to_assign;
		}
		else {
			num_to_assign = max_units;
			final_ang = ang;
		}
		for (int i = 0; i < num_to_assign; i++) {
			//assign from two ends to middle
			double a = m_ang + pow(-1, i % 2)*(final_ang / 2 - (i / 2)*ang_interval);
			int min_dist = MAXINT;
			BWAPI::Unit closest_unit = nullptr;
			tmp.x = (int)(cx + r * cos(a));
			tmp.y = (int)(cy + r * sin(a));
			for (auto & unit : unassigned){
				int d = unit->getDistance(tmp);
				if (d < min_dist){
					min_dist = d;
					closest_unit = unit;
				}
			}
			//if it's a unit far away from fight, do not assign it to a position
			if (closest_unit && min_dist > 300){
				unassigned.erase(closest_unit);
				continue;
			}
			if (tmp.isValid() && closest_unit){
				BWAPI::Broodwar->drawLineMap(closest_unit->getPosition(), tmp, BWAPI::Colors::Red);
				Micro::SmartMove(closest_unit, tmp);
				unassigned.erase(closest_unit);
				//find the total distance between unit and destination
				total_dest_dist += min_dist;
				num_assigned++;
			}		
		}
		r += 40;
		try_time++;
	}

	//if max destination distance less than 32, means forming has been finished
	if (num_assigned > 0 && total_dest_dist / num_assigned <= 32){
		return true;
	}
	else {
		BWAPI::Broodwar->drawTextScreen(200, 340, "%s", "Forming");
		return false;
	}
}

void MicroManager::regroup(const BWAPI::Position & regroupPosition) const
{
    BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());
    int regroupDistanceFromBase = MapTools::Instance().getGroundDistance(regroupPosition, ourBasePosition);

	// for each of the units we have
	for (auto & unit : _units)
	{
        int unitDistanceFromBase = MapTools::Instance().getGroundDistance(unit->getPosition(), ourBasePosition);

		// if the unit is outside the regroup area
        if (unitDistanceFromBase > regroupDistanceFromBase)
        {
            Micro::SmartMove(unit, ourBasePosition);
        }
		else if (unit->getDistance(regroupPosition) > 100)
		{
			// regroup it
			Micro::SmartMove(unit, regroupPosition);
		}
		else
		{
			Micro::SmartAttackMove(unit, regroupPosition);
		}
	}
}

bool MicroManager::unitNearEnemy(BWAPI::Unit unit)
{
	assert(unit);

	BWAPI::Unitset enemyNear;

	MapGrid::Instance().GetUnits(enemyNear, unit->getPosition(), 800, false, true);

	return enemyNear.size() > 0;
}

// returns true if position:
// a) is walkable
// b) doesn't have buildings on it
// c) doesn't have a unit on it that can attack ground
bool MicroManager::checkPositionWalkable(BWAPI::Position pos) 
{
	// get x and y from the position
	int x(pos.x), y(pos.y);

	// walkable tiles exist every 8 pixels
	bool good = BWAPI::Broodwar->isWalkable(x/8, y/8);
	
	// if it's not walkable throw it out
	if (!good) return false;
	
	// for each of those units, if it's a building or an attacking enemy unit we don't want to go there
	for (auto & unit : BWAPI::Broodwar->getUnitsOnTile(x/32, y/32)) 
	{
		if	(unit->getType().isBuilding() || unit->getType().isResourceContainer() || 
			(unit->getPlayer() != BWAPI::Broodwar->self() && unit->getType().groundWeapon() != BWAPI::WeaponTypes::None)) 
		{		
				return false;
		}
	}

	// otherwise it's okay
	return true;
}

void MicroManager::trainSubUnits(BWAPI::Unit unit) const
{
	if (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver)
	{
		unit->train(BWAPI::UnitTypes::Protoss_Scarab);
	}
	else if (unit->getType() == BWAPI::UnitTypes::Protoss_Carrier)
	{
		unit->train(BWAPI::UnitTypes::Protoss_Interceptor);
	}
}

bool MicroManager::unitNearChokepoint(BWAPI::Unit unit) const
{
	for (BWTA::Chokepoint * choke : BWTA::getChokepoints())
	{
		if (unit->getDistance(choke->getCenter()) < 80)
		{
			return true;
		}
	}

	return false;
}

void MicroManager::drawOrderText() 
{
	for (auto & unit : _units) 
    {
		if (Config::Debug::DrawUnitTargetInfo) BWAPI::Broodwar->drawTextMap(unit->getPosition().x, unit->getPosition().y, "%s", order.getStatus().c_str());
	}
}
