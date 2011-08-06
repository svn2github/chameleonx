/*
 * Copyright (c) 2011 Evan Lojewski. All rights reserved.
 *
 */
#ifndef DISK_H
#define DISK_H

#include <IOKit/IOTypes.h>
#include <Partition.hpp>

class Disk
{
public:
    Disk();
    Disk(const char* name);
    ~Disk();
        
    // TODO: add cacheing
    virtual IOReturn    Read(UInt64 sector, UInt64 size, UInt8* buffer) = 0;
    virtual IOReturn    Write(UInt64 sector, UInt64 size, UInt8* buffer) = 0;

    virtual bool        isValid()           { return mName != NULL && mBytesPerSector; };
    virtual bool        probe()             { return isValid(); };
    virtual UInt32      bytesPerSector()    { return mBytesPerSector; };
    
    virtual void        addPartition(Partition* partition);
    virtual Partition*  getPartition(UInt32 index);
	
	static Disk*		getDiskList() { return gFirstDisk; };
	virtual Disk*		getNextDisk() { return mNextDisk;  };
	virtual Disk*		getPrevDisk() { return mPrevDisk;  };
	
protected:
    PartitionList		*mPartitions;
    const char			*mName;
    const char			*mBusType;

    UInt32				mBytesPerSector;
	
	virtual void		setNextDisk(Disk* next) { mNextDisk = next; };
	virtual void		setPrevDisk(Disk* prev) { mPrevDisk = prev; };
private:
	Disk*			mPrevDisk;
	Disk*			mNextDisk;
	static Disk*	gFirstDisk;

};
#endif /* DISK_H */