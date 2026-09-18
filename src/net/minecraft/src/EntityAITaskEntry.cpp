#include "EntityAITaskEntry.h"

#include "EntityAIBase.h"

EntityAITaskEntry::EntityAITaskEntry(EntityAITasks *owner, int_t taskPriority, EntityAIBase *taskAction) :
	action(taskAction), priority(taskPriority), tasks(owner), executing(false)
{
}

EntityAITaskEntry::~EntityAITaskEntry()
{
	delete action;
}
