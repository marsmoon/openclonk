/**
	Flag Library
	The flagpoles mark the area a player owns. It also serves as an energy transmitter.
	
	@author Zapper
*/

static LibraryFlag_flag_list;

local DefaultFlagRadius = 200; // Radius of new flag of this type, unless overwritten by SetFlagRadius().
local lflag;

public func IsFlagpole(){return true;}

func RefreshAllFlagLinks()
{
	for(var f in LibraryFlag_flag_list) if (f)
	{
		f->ScheduleRefreshLinkedFlags();
	}
	
	// update power balance for power helpers after refreshing the linked flags
	Schedule(nil, "Library_Flag->RefreshAllPowerHelpers()", 2, 0);
	AddEffect("ScheduleRefreshAllPowerHelpers", nil, 1, 2, nil, Library_Flag);
}

func FxScheduleRefreshAllPowerHelpersTimer()
{
	Library_Flag->RefreshAllPowerHelpers();
	return -1;
}

// Refreshes all power networks (Library_Power objects).
func RefreshAllPowerHelpers()
{
	// Don't do anything if there are no power helpers created yet.
	if (GetType(LIB_POWR_Networks) != C4V_Array)
		return;
	
	// Special handling for neutral networks of which there should be at most one.
	for (var network in LIB_POWR_Networks)
	{
		if (!network || !network.lib_power.neutral_network) 
			continue;
		RefreshPowerHelper(network);
		break;
	}
	
	// Do the same for all other helpers: delete / refresh.
	for (var index = GetLength(LIB_POWR_Networks) - 1; index >= 0; index--)
	{
		var network = LIB_POWR_Networks[index];
		if (!network) 
			continue;
		
		if (network->IsEmpty())
		{
			network->RemoveObject();
			RemoveArrayIndex(LIB_POWR_Networks, index);
			continue;
		}
		//network->CheckPowerBalance();
	}
	return;
}

func RedrawFlagRadius()
{
	// Flag deactivated by setting empty radius
	if (!lflag.radius)
	{
		ClearFlagMarkers();
		return true;
	}
	
	//var flags = FindObjects(Find_ID(FlagPole),Find_Exclude(target), Find_Not(Find_Owner(GetOwner())), /*Find_Distance(FLAG_DISTANCE*2 + 10,0,0)*/Sort_Func("GetLifeTime"));
	var other_flags = [];
	var i = 0;
	for(var f in LibraryFlag_flag_list) if (f)
	{
		//if(f->GetOwner() == GetOwner()) continue;
		if(f == this) continue;
		other_flags[i++] = f;
	}
	// inner border
	var count = Max(5, lflag.radius / 10);
	var marker_index = -1;
	for(var i=0; i<360; i+= 360 / count)
	{
		++marker_index;
		var draw=true;
		var f=nil;
		var x= Sin(i, lflag.radius);
		var y=-Cos(i, lflag.radius);	
		//var inEnemy = false;
		
		for(var f in other_flags) if (f)
		{
			if(Distance(GetX()+x,GetY()+y,f->GetX(),f->GetY()) <= f->GetFlagRadius())
			{
				if(f->GetFlagConstructionTime() == GetFlagConstructionTime())
					lflag.construction_time += 1;
					
				if(f->GetFlagConstructionTime() < GetFlagConstructionTime())
				{	
					draw=false;
				}
				//else inEnemy=true;
				if(IsAllied(GetOwner(), f->GetOwner()))
					draw = false;
			}
		}
		
		if(!draw)
		{
			if(marker_index < GetLength(lflag.range_markers))
				if(lflag.range_markers[marker_index])
				{
					lflag.range_markers[marker_index]->FadeOut();
					lflag.range_markers[marker_index]->MoveTo(GetX(), GetY(), -lflag.range_markers[marker_index]->GetR());
				}
			continue;
		}
		var marker = lflag.range_markers[marker_index];
		if(!marker)
		{
			marker = CreateObjectAbove(GetFlagMarkerID(), 0, 0, GetOwner());
			marker->SetR(Angle(0, 0, x, y));
		}
		marker->FadeIn();
		marker->MoveTo(GetX() + x, GetY() + y, Angle(0, 0, x, y));
		lflag.range_markers[marker_index] = marker;
	}
	
	// there were unnecessary markers?
	if(marker_index < GetLength(lflag.range_markers) - 1)
	{
		var old = marker_index;
		while(++marker_index < GetLength(lflag.range_markers))
		{
			var marker = lflag.range_markers[marker_index];
			marker->RemoveObject();
			lflag.range_markers[marker_index] = nil;
		}
		SetLength(lflag.range_markers, old + 1);
	}
	
	return true;
}

func RefreshOwnershipOfSurrounding()
{
	for(var obj in FindObjects(Find_Distance(lflag.radius), Find_Func("CanBeOwned")))
	{
		var o = GetOwnerOfPosition(AbsX(obj->GetX()), AbsY(obj->GetY()));
		if(obj->GetOwner() == o) continue;
		var old = obj->GetOwner();
		obj->SetOwner(o);
	}
}
public func Initialize()
{
	AddOwnership();
	AddEffect("IntFlagMovementCheck", this, 100, 12, this);
	return _inherited(...);
}

public func Construction()
{
	if(GetType(LibraryFlag_flag_list) != C4V_Array)
		LibraryFlag_flag_list = [];
	
	lflag =
	{
		construction_time = FrameCounter(),
		radius = GetID()->GetFlagRadius(),
		range_markers = [],
		linked_flags = [],
		power_helper = nil
	};
		
	return _inherited(...);
}

public func Destruction()
{
	// Important: first process other libraries like the power library which removes links
	// from the network and then handle ownership removal.
	_inherited(...);
	RemoveOwnership();
	return;
}

private func AddOwnership()
{
	if(GetIndexOf(LibraryFlag_flag_list, this) == -1)
		LibraryFlag_flag_list[GetLength(LibraryFlag_flag_list)] = this;

	// redraw
	RedrawAllFlagRadiuses();
	
	// ownership
	RefreshOwnershipOfSurrounding();
	
	// linked flags - optimization for power system
	RefreshAllFlagLinks();
}

private func RemoveOwnership()
{
	ClearFlagMarkers();
	
	// remove from global array
	for(var i = 0; i < GetLength(LibraryFlag_flag_list); ++i)
	{
		if(LibraryFlag_flag_list[i] != this) continue;
		LibraryFlag_flag_list[i] = LibraryFlag_flag_list[GetLength(LibraryFlag_flag_list)-1];
		SetLength(LibraryFlag_flag_list, GetLength(LibraryFlag_flag_list)-1);
		break;
	}
	
	// redraw
	RedrawAllFlagRadiuses();
	
	// ownership
	RefreshOwnershipOfSurrounding();
	
	// refresh all flag links
	RefreshAllFlagLinks();
}

protected func FxIntFlagMovementCheckStart(object target, proplist effect, int temp)
{
	if (temp)
		return FX_OK;
	effect.moving = false;
	effect.Interval = 12;
	effect.X = target->GetX();
	effect.Y = target->GetY();
	return FX_OK;
}

protected func FxIntFlagMovementCheckTimer(object target, proplist effect)
{
	// Check if flag started moving.
	if (!effect.moving)
	{
		if (effect.X != target->GetX() || effect.Y != target->GetY())
		{
			effect.moving = true;
			RemoveOwnership();
		}	
	}
	// Check if flag stopped moving.
	else
	{
		if (effect.X == target->GetX() && effect.Y == target->GetY())
		{
			effect.moving = false;
			AddOwnership();
		}	
	}
	// Update coordinates.
	effect.X = target->GetX();
	effect.Y = target->GetY();
	return FX_OK;
}

func ScheduleRefreshLinkedFlags()
{
	if(GetEffect("RefreshLinkedFlags", this)) return;
	AddEffect("RefreshLinkedFlags", this, 1, 1, this);
}

func StopRefreshLinkedFlags()
{
	if(!GetEffect("RefreshLinkedFlags", this)) return;
	RemoveEffect("RefreshLinkedFlags", this);
}

func FxRefreshLinkedFlagsTimer()
{
	this->RefreshLinkedFlags();
	return -1;
}

// Returns all flags allied to owner of which the radius intersects the given circle
func FindFlagsInRadius(object center_object, int radius, int owner)
{
	var flag_list = [];
	if (LibraryFlag_flag_list) for(var flag in LibraryFlag_flag_list)
	{
		if(!IsAllied(flag->GetOwner(), owner)) continue;
		if(flag == center_object) continue;
		if(ObjectDistance(center_object, flag) > radius + flag->GetFlagRadius()) continue;
		flag_list[GetLength(flag_list)] = flag;
	}
	return flag_list;
}

func RefreshLinkedFlags()
{
	// failsafe - the array should exist
	if(GetType(LibraryFlag_flag_list) != C4V_Array) return;
	
	var current = [];
	var new = [this];
	
	var owner = GetOwner();
	
	while(GetLength(new))
	{
		for(var f in new) if(f != this) current[GetLength(current)] = f;
		var old = new;
		new = [];
		
		for(var oldflag in old)
		for(var flag in LibraryFlag_flag_list)
		{
			if(!IsAllied(flag->GetOwner(), owner)) continue;
			if(GetIndexOf(current, flag) != -1) continue;
			if(GetIndexOf(new, flag) != -1) continue;
			if(flag == this) continue;
			
			if(ObjectDistance(oldflag, flag) > oldflag->GetFlagRadius() + flag->GetFlagRadius()) continue;
			
			new[GetLength(new)] = flag;
		}
	}
	
	lflag.linked_flags = current;
	
	// update flag links for all linked flags - no need for every flag to do that
	// meanwhile, adjust power helper. Merge if necessary
	// since we don't know whether flag links have been lost we will create a new power helper and possibly remove old ones
	Library_Power->Init(); // make sure the power system is set up
	var old = lflag.power_helper;
	lflag.power_helper = CreateObject(Library_Power, 0, 0, NO_OWNER);
	LIB_POWR_Networks[GetLength(LIB_POWR_Networks)] = lflag.power_helper;
	
	// list of helpers yet to merge
	var to_merge = [old];
	
	// copy from me
	lflag.linked_flags = current;
	for (var other in lflag.linked_flags)
	{
		other->CopyLinkedFlags(this, lflag.linked_flags);
		
		if (GetIndexOf(to_merge, other.lflag.power_helper) == -1)
			to_merge[GetLength(to_merge)] = other.lflag.power_helper;
		other.lflag.power_helper = lflag.power_helper;
	}
	
	// for every object in to_merge check if it actually (still) belongs to the group
	for (var h in to_merge)
	{
		if (h == nil)
			continue;
		RefreshPowerHelper(h);
	}
}

func RefreshPowerHelper(object network)
{
	// Merge all the producers and consumers into their actual networks.
	for (var link in Concatenate(network.lib_power.idle_producers, network.lib_power.active_producers))
	{
		if (!link)
			continue;
		var actual_network = Library_Power->GetPowerNetwork(link.obj);
		if (!actual_network || actual_network == network)
			continue;
		// Remove from old network and add to new network.
		network->RemovePowerProducer(link.obj);
		actual_network->AddPowerProducer(link.obj, link.prod_amount, link.priority);
	}
	for (var link in Concatenate(network.lib_power.waiting_consumers, network.lib_power.active_consumers))
	{
		if (!link)
			continue;
		var actual_network = Library_Power->GetPowerNetwork(link.obj);
		if (!actual_network || actual_network == network)
			continue;
		// Remove from old network and add to new network.
		network->RemovePowerConsumer(link.obj);
		actual_network->AddPowerConsumer(link.obj, link.cons_amount, link.priority);
	}
	return;
}

public func CopyLinkedFlags(object from, array flaglist)
{
	lflag.linked_flags = flaglist[:];
	for(var i = GetLength(lflag.linked_flags)-1; i >= 0; --i)
		if(lflag.linked_flags[i] == this)
			lflag.linked_flags[i] = from;
	StopRefreshLinkedFlags();
}

public func GetPowerHelper(){return lflag.power_helper;}
public func SetPowerHelper(object to){lflag.power_helper = to;}

public func GetLinkedFlags(){return lflag.linked_flags;}

private func ClearFlagMarkers()
{
	for(var obj in lflag.range_markers)
		if (obj) obj->RemoveObject();
	lflag.range_markers = [];
}

public func SetFlagRadius(int to)
{
	lflag.radius = to;
	
	// redraw
	RedrawAllFlagRadiuses();
	
	// ownership
	RefreshOwnershipOfSurrounding();
	
	return true;
}

// reassign owner of flag markers for correct color
func OnOwnerChanged(int new_owner, int old_owner)
{
	for(var marker in lflag.range_markers)
	{
		if(!marker) continue;
		marker->SetOwner(new_owner);
		marker->ResetColor();
	}
}

// callback from construction library: Create a special preview that gives extra info about affected buildings / flags
func CreateConstructionPreview(object constructing_clonk)
{
	return CreateObjectAbove(Library_Flag_ConstructionPreviewer, constructing_clonk->GetX()-GetX(), constructing_clonk->GetY()-GetY(), constructing_clonk->GetOwner());
}

public func GetFlagRadius(){if (lflag) return lflag.radius; else return DefaultFlagRadius;}
public func GetFlagConstructionTime() {return lflag.construction_time;}
public func GetFlagMarkerID(){return LibraryFlag_Marker;}

public func SaveScenarioObject(props)
{
	if (!inherited(props, ...)) return false;
	props->Remove("Category"); // category is always set to StaticBack...
	if (lflag && lflag.radius != DefaultFlagRadius) props->AddCall("Radius", this, "SetFlagRadius", lflag.radius);
	return true;
}
