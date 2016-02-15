#pragma once

#include "Common.h"
#include "MapGrid.h"
#include "SquadOrder.h"
#include "MapTools.h"
#include "InformationManager.h"
#include "Micro.h"

namespace UAlbertaBot
{
struct AirThreat
{
	BWAPI::Unit	unit;
	double			weight;
};

struct GroundThreat
{
	BWAPI::Unit	unit;
	double			weight;
};

class MicroManager
{
	BWAPI::Unitset  _units;
	bool _formation;

protected:
	
	SquadOrder			order;

	virtual void        executeMicro(const BWAPI::Unitset & targets) = 0;
	bool                checkPositionWalkable(BWAPI::Position pos);
	void                drawOrderText();
	bool                unitNearEnemy(BWAPI::Unit unit);
	bool                unitNearChokepoint(BWAPI::Unit unit) const;
	void                trainSubUnits(BWAPI::Unit unit) const;
    

public:
	bool				formed;

						MicroManager();
    virtual				~MicroManager(){}

	const BWAPI::Unitset & getUnits() const;
	bool MicroManager::getFormation();
	bool MicroManager::formSquad(const BWAPI::Unitset & targets, int dist, int radius, double angle, int interval);
	BWAPI::Position     calcCenter() const;

	void				setUnits(const BWAPI::Unitset & u);
	void				setFormation(bool f);
	void				execute(const SquadOrder & order);
	void				regroup(const BWAPI::Position & regroupPosition) const;

};
}