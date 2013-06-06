// @ZBS {
//		+DESCRIPTION {
//			DIstributed Work Order Server
//		}
//		*MODULE_DEPENDS diwos.cpp
//		*INTERFACE console
// }

// OPERATING SYSTEM specific includes:
// SDK includes:
// STDLIB includes:
#include "stdio.h"
#ifdef WIN32
	#include "conio.h"
	#include "malloc.h"
#endif
#include "string.h"
#include "assert.h"
#include "stdlib.h"
#include "stdarg.h"
// MODULE includes:
// ZBSLIB includes:
#include "zmsgzocket.h"
#include "ztmpstr.h"
#include "zcmdparse.h"
#include "zconsole.h"
#include "zconfig.h"
#include "ztime.h"
#include "ztl.h"
#include "zfilelock.h"
#include "zrand.h"

void trace( char *fmt, ... ) {
	va_list argptr;
	va_start( argptr, fmt );
	vfprintf( stdout, fmt, argptr );
	va_end( argptr );
	fflush( stdout );
}


// Data structures
//-----------------------------------------------------------------------------------------

struct Account {
	char loginName[64];
	char password[64];
	int workOrdersCompleted;
	int workOrdersRequested;
	int activeWorkOrderCount;

	Account() {
		loginName[0] = 0;
		password[0] = 0;
		workOrdersCompleted = 0;
		workOrdersRequested = 0;
		activeWorkOrderCount = 0;
	}
};

struct Job {
	static int jobIdSerial;
	int jobId;
	char protocol[64];
	int workOrdersRequested;
	int workOrdersSent;
	int workOrdersCompleted;
	int workOrdersDelivered;
	ZTimeUTC totalComputationTime;
	ZTimeUTC whenCreated;
	char creator[64];

	Job() {
		jobId = ++jobIdSerial;
		protocol[0] = 0;
		workOrdersRequested = 0;
		workOrdersSent = 0;
		workOrdersCompleted = 0;
		workOrdersDelivered = 0;
		totalComputationTime = 0;
		whenCreated = zTimeUTC();
		creator[0] = 0;
	}

	void save( FILE * f ) {
		fwrite( this, sizeof(*this), 1, f );
	}

	void load( FILE * f ) {
		fread( this, sizeof(*this), 1, f );
	}
};

int Job::jobIdSerial = 1;

struct WorkOrder {
	ZHashTable instructions;
	ZHashTable results;
	int jobId;
	int workOrderId;
	char protocol[64];

	ZTimeUTC whenRequested;
	ZTimeUTC whenSent;
	ZTimeUTC whenCompleted;
	ZTimeUTC whenDelivered;

	int assumeAbandonedAfterSeconds;

	int sentUniqId;
	char sentWhoToCredit[64];

	WorkOrder() {
		jobId = 0;
		workOrderId = 0;
		protocol[0] = 0;

		whenRequested = zTimeUTC();
		whenSent = 0;
		whenCompleted = 0;
		whenDelivered = 0;

		assumeAbandonedAfterSeconds = 0;

		sentUniqId = 0;
		sentWhoToCredit[0] = 0;
	}

	void save( FILE * f ) {
		fwrite( &jobId, sizeof(*this) - ( (char*)&jobId - (char*)this ), 1, f );

		unsigned int size = 0;
		char *buf = zHashTablePack( instructions, &size );
		fwrite( buf, size, 1, f );
		free( buf );

		buf = zHashTablePack( results, &size );
		fwrite( buf, size, 1, f );
		free( buf );
	}

	void load( FILE * f ) {
		assert( sizeof(int) == 4 );
		fread( &jobId, sizeof(*this) - ( (char*)&jobId - (char*)this ), 1, f );

		int size = 0;
		fread( &size, 4, 1, f );
		fseek( f, -4, SEEK_CUR );
		char *buf = (char *)malloc( size );
		fread( buf, size, 1, f );
		zHashTableUnpack( buf, instructions );
		free( buf );

		fread( &size, 4, 1, f );
		fseek( f, -4, SEEK_CUR );
		buf = (char *)malloc( size );
		fread( buf, size, 1, f );
		zHashTableUnpack( buf, results );
		free( buf );
	}
};

struct Connection {
	int zocketId;
	ZTimeUTC connectTime;
	int availForWork;
	int monitorJobId;
	char loginName[64];
	char *protocols;
	ZTimeUTC markedForDelete;

	Connection( int id ) {
		zocketId = id;
		connectTime = zTimeUTC();
		availForWork = 0;
		monitorJobId = 0;
		loginName[0] = 0;
		protocols = 0;
		markedForDelete = 0;
	}

	~Connection() {
		if( protocols ) {
			free( protocols );
		}
	}
};

struct Wad {
	int jobId;
	char wadName[64];
	ZHashTable wad;

	void save( FILE * f ) {
		fwrite( &jobId, sizeof(jobId), 1, f );
		fwrite( wadName, sizeof(wadName), 1, f );

		unsigned int size = 0;
		char *buf = zHashTablePack( wad, &size );
		fwrite( buf, size, 1, f );
		free( buf );
	}

	void load( FILE * f ) {
		fread( &jobId, sizeof(jobId), 1, f );
		fread( wadName, sizeof(wadName), 1, f );

		int size = 0;
		fread( &size, sizeof(int), 1, f );
		fseek( f, -4, SEEK_CUR );
		char *buf = (char *)malloc( size );
		fread( buf, size, 1, f );
		zHashTableUnpack( buf, wad );
		free( buf );
	}
};


ZTLVec< Account * > accounts;
ZTLVec< Job * > jobs;
ZTLVec< WorkOrder * > workOrders;
ZTLVec< Connection * > connections;
ZTLVec< Wad * > wads;


Account *accountFind( char *loginName ) {
	for( int i=0; i<accounts.count; i++ ) {
		if( !strcmp(accounts[i]->loginName,loginName) ) {
			return accounts[i];
		}
	}
	return 0;
}

Job *jobFind( int jobId ) {
	for( int i=0; i<jobs.count; i++ ) {
		if( jobs[i]->jobId == jobId ) {
			return jobs[i];
		}
	}
	return 0;
}

Connection *connectionFind( int zocketId ) {
	for( int i=0; i<connections.count; i++ ) {
		if( connections[i]->zocketId == zocketId ) {
			return connections[i];
		}
	}
	return 0;
}

void connectionDelete( int zocketId ) {
	for( int i=0; i<connections.count; i++ ) {
		if( connections[i]->zocketId == zocketId ) {
			delete connections[i];
			connections.del( i );
			break;
		}
	}
}

WorkOrder *workOrderFind( int jobId, int workOrderId ) {
	for( int i=0; i<workOrders.count; i++ ) {
		if( workOrders[i]->jobId == jobId && workOrders[i]->workOrderId == workOrderId ) {
			return workOrders[i];
		}
	}
	return 0;
}

Wad *wadFind( int jobId, char *wadName ) {
	for( int i=0; i<wads.count; i++ ) {
		Wad *w = wads[i];
		if( w->jobId == jobId && !strcmp(w->wadName,wadName) ) {
			return w;
		}
	}
	return 0;
}

void saveAll() {
	int i;

	// SAVE accounts
	FILE *f = fopen( "_diwos/accounts.txt", "wt" );
	for( i=0; i<accounts.count; i++ ) {
		Account *a = accounts[i];
		fprintf( f, "%s %s %d %d\n", a->loginName, a->password, a->workOrdersCompleted, a->workOrdersRequested );
	}
	fclose( f );

	f = fopen( "_diwos/state.dat", "wb" );

	// SAVE jobs
	fwrite( &jobs.count, sizeof(jobs.count), 1, f );
	for( i=0; i<jobs.count; i++ ) {
		jobs[i]->save( f );
	}

	// SAVE workOrders
	fwrite( &workOrders.count, sizeof(workOrders.count), 1, f );
	for( i=0; i<workOrders.count; i++ ) {
		workOrders[i]->save( f );
	}

	// SAVE wads
	fwrite( &wads.count, sizeof(wads.count), 1, f );
	for( i=0; i<wads.count; i++ ) {
		wads[i]->save( f );
	}

	fclose( f );
}

void loadAll() {
	int i;

	// @TODO: This will eventually be a prperly secure MD5 hashed password, etc
	FILE *f = fopen( "_diwos/accounts.txt", "rt" );
	assert( f );
	Account temp;
	while( fscanf( f, "%s %s %d %d\n", temp.loginName, temp.password, &temp.workOrdersCompleted, &temp.workOrdersRequested ) == 4 ) {
		Account *a = new Account;
		memcpy( a, &temp, sizeof(temp) );
		accounts.add( a );
	}
	fclose( f );

	f = fopen( "_diwos/state.dat", "rb" );
	if( f ) {
		// LOAD jobs
		int jobsCount;
		fread( &jobsCount, sizeof(jobsCount), 1, f );
		for( i=0; i<jobsCount; i++ ) {
			Job *temp = new Job;
			temp->load( f );
			jobs.add( temp );
		}

		// LOAD workOrders
		int workOrdersCount;
		fread( &workOrdersCount, sizeof(workOrdersCount), 1, f );
		for( i=0; i<workOrdersCount; i++ ) {
			WorkOrder *temp = new WorkOrder;
			temp->load( f );
			workOrders.add( temp );
		}

		// LOAD wads
		int wadCount;
		fread( &wadCount, sizeof(wadCount), 1, f );
		for( i=0; i<wadCount; i++ ) {
			Wad *temp = new Wad;
			temp->load( f );
			wads.add( temp );
		}

		fclose( f );
	}

	// UPDATE the active work order count
	for( i=0; i<accounts.count; i++ ) {
		accounts[i]->activeWorkOrderCount = 0;
	}

	for( i=0; i<workOrders.count; i++ ) {
		Job *j = jobFind( workOrders[i]->jobId );
		if( j ) {
			Account *a = accountFind( j->creator );
			if( a ) {
				a->activeWorkOrderCount++;
			}
		}
	}
}

// Connect, Disconnect, Login
//-----------------------------------------------------------------------------------------

ZMSG_HANDLER( Connect ) {
	int id = zmsgI( fromRemote );
	trace( "Connect %d\n", id );
	Connection *c = new Connection( id );
	connections.add( c );
}

ZMSG_HANDLER( Disconnect ) {
	int id = zmsgI( fromRemote );
	trace( "Disconnect %d\n", id );
	connectionDelete( id );
}

ZMSG_HANDLER( Login ) {
	int id = zmsgI( fromRemote );
	char *loginName = zmsgS( loginName );
	char *password = zmsgS( password );

	trace( "Login %s %s\n", loginName, password );

	Connection *c = connectionFind( id );
	assert( c );

	// CHECK the login information
	int success = 0;
	Account *a = accountFind( loginName );
	if( strlen(loginName) < sizeof(a->loginName) && a ) {
		if( ! strcmp( password, a->password ) ) {
			strcpy( c->loginName, loginName );
			success = 1;
		}
	}

	if( success ) {
		zMsgQueue( "type=LoginSuccess toRemote=%d", id );
	}
	else {
		zMsgQueue( "type=LoginFailed toRemote=%d", id );
		c->markedForDelete = zTimeUTC() + 10;
	}
}

// Job and WorkOrder messages
//-----------------------------------------------------------------------------------------

ZMSG_HANDLER( AvailableForWork ) {
	int id = zmsgI( fromRemote );
	char *protocols = zmsgS(protocols);

	Connection *c = connectionFind( id );
	assert( c );

	c->availForWork = 1;
	c->protocols = strdup( protocols );

	trace( "AvailableForWork %d %s %s %d %s\n", id, c->loginName, protocols, c->availForWork, c->protocols );
}

ZMSG_HANDLER( JobCreate ) {
	int id = zmsgI( fromRemote );
	char *protocol = zmsgS( protocol );

	Connection *c = connectionFind( id );
	assert( c );
		// Shouldn't ever be the case that I get a message that wasn't from someone that I know about.

	char *reason = "";
	int success = 0;

	Account *a = accountFind( c->loginName );
	if( a ) {
		Job *j = new Job;

		if( strlen(protocol) < sizeof(j->protocol)-1 ) {

			if( protocol[0] != '*' ) {
				// ADD a * delimiter
				j->protocol[0] = '*';
				strcpy( &j->protocol[1], protocol );
			}
			else {
				strcpy( j->protocol, protocol );
			}

			strcpy( j->creator, c->loginName );
			
			c->monitorJobId = j->jobId;
			
			jobs.add( j );

			trace( "JobCreate %d %d\n", id, j->jobId );
			zMsgQueue( "type=JobCreated jobId=%d toRemote=%d", j->jobId, id );
			success = 1;
		}
		else {
			delete j;
			reason = "bad protocol or name";
		}
	}

	if( !success ) {
		zMsgQueue( "type=JobCreateFailed toRemote=%d reason='%s'", id, reason );
		c->markedForDelete = zTimeUTC() + 10;
	}

	saveAll();
}

ZMSG_HANDLER( JobDelete ) {
	int i;
	int id = zmsgI( fromRemote );
	int jobId = zmsgI( jobId );

	trace( "JobDelete %d\n", jobId );

	// FIND all of the work orders assocaited with this job and delete them
	for( i=0; i<workOrders.count; i++ ) {
		WorkOrder *w = workOrders[i];
		if( w->jobId == jobId ) {

			Job *j = jobFind( jobId );
			if( j ) {
				Account *a = accountFind( j->creator );
				if( a ) {
					a->activeWorkOrderCount--;
				}
			}

			delete w;
			workOrders.del( i );
			i--;
		}
	}

	// FIND all of the wads orders associated with this job and delete them
	for( i=0; i<wads.count; i++ ) {
		Wad *w = wads[i];
		if( w->jobId == jobId ) {
			delete w;
			wads.del( i );
			i--;
		}
	}

	// DELETE the job
	for( i=0; i<jobs.count; i++ ) {
		if( jobs[i]->jobId == jobId ) {
			delete jobs[i];
			jobs.del( i );
			i--;
			break;
		}
	}

	saveAll();

	zMsgQueue( "type=JobDeleteAck toRemote=%d", id );
}

ZMSG_HANDLER( JobStatusRequest ) {
	int id = zmsgI( fromRemote );
	int jobId = zmsgI( jobId );

	trace( "JobStatusRequest %d\n", jobId );

	ZMsg *reply = new ZMsg;
	reply->putS( "type", "JobStatusReply" );
	reply->putI( "toRemote", id );

	Job *j = jobFind( jobId );
	if( j ) {
		reply->putI( "jobId", j->jobId );
		reply->putS( "protocol", j->protocol );
		reply->putI( "workOrdersRequested", j->workOrdersRequested );
		reply->putI( "workOrdersSent", j->workOrdersSent );
		reply->putI( "workOrdersCompleted", j->workOrdersCompleted );
		reply->putI( "workOrdersDelivered", j->workOrdersDelivered );
		reply->putD( "totalComputationTime", j->totalComputationTime );
		reply->putD( "whenCreated", j->whenCreated );
		reply->putS( "creator", j->creator );

		int index = 0;
		for( int i=0; i<workOrders.count; i++ ) {
			WorkOrder *w = workOrders[i];
			if( w->jobId == jobId ) {
				reply->putI( ZTmpStr("wo-%d-id",index), w->workOrderId );
				reply->putD( ZTmpStr("wo-%d-whenRequested",index), w->whenRequested );
				reply->putD( ZTmpStr("wo-%d-whenSent",index), w->whenSent );
				reply->putD( ZTmpStr("wo-%d-whenCompleted",index), w->whenCompleted );
				reply->putD( ZTmpStr("wo-%d-whenDelivered",index), w->whenDelivered );

				index++;
			}
		}
		reply->putI( "wo-count", index );
		reply->putS( "result", "Job found" );
	}
	else {
		reply->putS( "result", "Job not found" );
	}

	zMsgQueue( reply );
}

ZMSG_HANDLER( JobDeadListRequest ) {
	int id = zmsgI( fromRemote );

	trace( "JobListRequest %d\n", id );

	ZMsg *reply = new ZMsg;
	reply->putS( "type", "JobDeadListReply" );
	reply->putI( "toRemote", id );

	for( ZHashWalk n( *msg ); n.next(); ) {
		int jobId = atoi( n.key );
		if( jobId ) {
			Job *j = jobFind( jobId );
			if( !j ) {
				reply->putI( n.key, 1 );
			}
		}
	}

	zMsgQueue( reply );
}

ZMSG_HANDLER( JobMonitor ) {
	int id = zmsgI( fromRemote );
	int jobId = zmsgI( jobId );

	Connection *c = connectionFind( id );
	if( c ) {
		c->monitorJobId = jobId;
	}
}

ZMSG_HANDLER( WorkOrderCreate ) {
	int id = zmsgI( fromRemote );
	int jobId = zmsgI( jobId );
	int workOrderId = zmsgI( workOrderId );

	trace( "WorkOrderCreate %d %d\n", jobId, workOrderId );

	Job *j = jobFind( jobId );
	if( j ) {
		WorkOrder *w = new WorkOrder;
		w->jobId = jobId;
		w->instructions.copyFrom( *msg );
		w->workOrderId = workOrderId;
		strcpy( w->protocol, j->protocol );

		j->workOrdersRequested++;

		Account *a = accountFind( j->creator );
		if( a ) {
			a->workOrdersRequested++;
			a->activeWorkOrderCount++;
		}

		w->assumeAbandonedAfterSeconds = zmsgI(assumeAbandonedAfterSeconds);
		if( w->assumeAbandonedAfterSeconds <= 0 ) {
			// Default to an hour of computation time if none is given
			w->assumeAbandonedAfterSeconds = 3600;
		}

		workOrders.add( w );
	}

	saveAll();
}

ZMSG_HANDLER( WorkOrderComplete ) {
	int id = zmsgI( fromRemote );
	int workOrderUniqId = zmsgI( workOrderUniqId );

	trace( "WorkOrderComplete %d\n", workOrderUniqId );

	for( int i=0; i<workOrders.count; i++ ) {
		WorkOrder *w = workOrders[i];
		if( w->sentUniqId == workOrderUniqId ) {
			w->results.copyFrom( *msg );
			w->whenCompleted = zTimeUTC();

			// TALLY job statistics
			Job *j = jobFind( w->jobId );
			if( j ) {
				j->workOrdersCompleted++;
				j->totalComputationTime += w->whenCompleted - w->whenSent;
			}

			// CREDIT account
			Account *a = accountFind( w->sentWhoToCredit );
			if( a ) {
				a->workOrdersCompleted++;
			}

			a = accountFind( j->creator );
			if( a ) {
				a->activeWorkOrderCount--;
			}

			// INFORM connections montioring this job
			for( int ci=0; ci<connections.count; ci++ ) {
				if( connections[ci]->monitorJobId == w->jobId ) {
					zMsgQueue( "type=WorkOrderAvailable jobId=%d workOrderId=%d toRemote=%d", w->jobId, w->workOrderId, connections[ci]->zocketId );
				}
			}

			break;
		}
	}

	saveAll();
}

ZMSG_HANDLER( WorkOrderDeliverRequest ) {
	int id = zmsgI( fromRemote );
	int jobId = zmsgI( jobId );
	int workOrderId = zmsgI( workOrderId );

	ZMsg *woDeliver = new ZMsg;

	WorkOrder *wo = workOrderFind( jobId, workOrderId );
	if( wo ) {
		wo->whenDelivered = zTimeUTC();

		woDeliver->copyFrom( wo->results );
		woDeliver->putS( "type", "WorkOrderDeliver" );
		woDeliver->putI( "jobId", jobId );
		woDeliver->putI( "workOrderId", workOrderId );
		woDeliver->putI( "toRemote", id );
		woDeliver->putI( "whenDelivered", wo->whenDelivered );
		zMsgQueue( woDeliver );

		Job *j = jobFind( jobId );
		j->workOrdersDelivered++;
	}
	else {
		woDeliver->putS( "type", "WorkOrderDeliverFail" );
		woDeliver->putI( "jobId", jobId );
		woDeliver->putI( "workOrderId", workOrderId );
		woDeliver->putI( "toRemote", id );
		zMsgQueue( woDeliver );
	}

	saveAll();
}

ZMSG_HANDLER( WadCreate ) {
	int id = zmsgI( fromRemote );
	int jobId = zmsgI( jobId );
	char *wadName = zmsgS( wadName );

	Wad *w = wadFind( jobId, wadName );
	if( ! w ) {
		Job *j = jobFind( jobId );
		if( j ) {
			Wad *w = new Wad;
			w->jobId = jobId;
			strncpy( w->wadName, wadName, sizeof(w->wadName)-1 );
			w->wadName[sizeof(w->wadName)-1] = 0;
			w->wad.copyFrom( *msg );
			wads.add( w );
		}
	}
}

ZMSG_HANDLER( WadRequest ) {
	int id = zmsgI( fromRemote );
	int jobId = zmsgI( jobId );
	char *wadName = zmsgS( wadName );

	ZMsg *newMsg = new ZMsg;

	Wad *w = wadFind( jobId, wadName );
	if( w ) {
		newMsg->copyFrom( w->wad );
	}
	else {
		newMsg->putS( "wadError", "Not found" );
	}
	newMsg->putS( "type", "WadReply" );
	newMsg->putI( "toRemote", id );
	zMsgQueue( newMsg );
}


// Command processor
//-----------------------------------------------------------------------------------------

void msgPrintF( ZMsg *msg, char *fmt, ... ) {
	char temp[4096];
	va_list argptr;
	va_start( argptr, fmt );
	vsprintf( temp, fmt, argptr );
	va_end( argptr );

	char *text = msg->getS( "text", "" );
	int textLen = strlen( text );
	int tempLen = strlen( temp );
	char *newBuf = (char *)alloca( textLen+tempLen+1 );
	memcpy( newBuf, text, textLen );
	memcpy( &newBuf[textLen], temp, tempLen+1 );

	msg->putS( "text", newBuf );
}

ZMSG_HANDLER( CommandConn ) {
	int id = zmsgI( fromRemote );
	ZMsg *reply = new ZMsg;

	ZTimeUTC now = zTimeUTC();

	for( int i=0; i<ZMsgZocket::listCount; i++ ) {
		ZMsgZocket *z = ZMsgZocket::list[i];
		if( z->isListening() ) {
			reply->putI( ZTmpStr("%d/zid",i), z->id );
			reply->putI( ZTmpStr("%d/listening",i), 1 );
		}
		else {
			Connection *c = connectionFind( z->id );
			if( c ) {
				ZocketAddress zAddr = z->getRemoteAddress();
				reply->putI( ZTmpStr("%d/zid",i), z->id );
				reply->putS( ZTmpStr("%d/addr",i), zAddr.encode() );
				reply->putI( ZTmpStr("%d/connectDuration",i), now - c->connectTime );
				reply->putI( ZTmpStr("%d/availForWork",i), c->availForWork );
				reply->putI( ZTmpStr("%d/monitorJobId",i), c->monitorJobId );
				reply->putS( ZTmpStr("%d/loginName",i), c->loginName );
				reply->putS( ZTmpStr("%d/protocols",i), c->protocols );
				reply->putI( ZTmpStr("%d/markedForDelete",i), c->markedForDelete );
			}
			else {
				reply->putI( ZTmpStr("%d/zid",i), z->id );
				reply->putI( ZTmpStr("%d/orphaned",i), 1 );
			}
		}
	}

	reply->putI( "toRemote", id );
	zMsgQueue( reply );
}

ZMSG_HANDLER( CommandJobs ) {
	int id = zmsgI( fromRemote );
	ZMsg *reply = new ZMsg;

	for( int i=0; i<jobs.count; i++ ) {
		Job *j = jobs[i];
		reply->putI( ZTmpStr("%d/jobId",i), j->jobId );
		reply->putS( ZTmpStr("%d/protocol",i), j->protocol );
		reply->putI( ZTmpStr("%d/workOrdersRequested",i), j->workOrdersRequested );
		reply->putI( ZTmpStr("%d/workOrdersSent",i), j->workOrdersSent );
		reply->putI( ZTmpStr("%d/workOrdersCompleted",i), j->workOrdersCompleted );
		reply->putI( ZTmpStr("%d/workOrdersDelivered",i), j->workOrdersDelivered );
		reply->putI( ZTmpStr("%d/totalComputationTime",i), j->totalComputationTime );
		reply->putI( ZTmpStr("%d/whenCreated",i), j->whenCreated );
		reply->putS( ZTmpStr("%d/creator",i), j->creator );
	}

	reply->putI( "toRemote", id );
	zMsgQueue( reply );
}

ZMSG_HANDLER( CommandWork ) {
	int id = zmsgI( fromRemote );
	ZMsg *reply = new ZMsg;

	for( int i=0; i<workOrders.count; i++ ) {
		WorkOrder *w = workOrders[i];
		reply->putI( ZTmpStr("%d/instructionsCount",i), w->instructions.activeCount() );
		reply->putI( ZTmpStr("%d/resultsCount",i), w->results.activeCount() );
		reply->putI( ZTmpStr("%d/jobId",i), w->jobId );
		reply->putI( ZTmpStr("%d/workOrderId",i), w->workOrderId );
		reply->putS( ZTmpStr("%d/protocol",i), w->protocol );
		reply->putI( ZTmpStr("%d/whenRequested",i), w->whenRequested );
		reply->putI( ZTmpStr("%d/whenSent",i), w->whenSent );
		reply->putI( ZTmpStr("%d/whenCompleted",i), w->whenCompleted );
		reply->putI( ZTmpStr("%d/whenDelivered",i), w->whenDelivered );
		reply->putI( ZTmpStr("%d/assumeAbandonedAfterSeconds",i), w->assumeAbandonedAfterSeconds );
		reply->putI( ZTmpStr("%d/sentUniqId",i), w->sentUniqId );
		reply->putS( ZTmpStr("%d/sentWhoToCredit",i), w->sentWhoToCredit );
	}

	reply->putI( "toRemote", id );
	zMsgQueue( reply );
}

ZMSG_HANDLER( CommandActs ) {
	int id = zmsgI( fromRemote );
	ZMsg *reply = new ZMsg;

	for( int i=0; i<accounts.count; i++ ) {
		Account *a = accounts[i];
		reply->putS( ZTmpStr("%d/loginName",i), a->loginName );
		reply->putI( ZTmpStr("%d/workOrdersRequested",i), a->workOrdersRequested );
		reply->putI( ZTmpStr("%d/workOrdersCompleted",i), a->workOrdersCompleted );
		reply->putI( ZTmpStr("%d/activeWorkOrderCount",i), a->activeWorkOrderCount );
	}

	reply->putI( "toRemote", id );
	zMsgQueue( reply );
}

ZMSG_HANDLER( CommandWads ) {
	int id = zmsgI( fromRemote );
	ZMsg *reply = new ZMsg;

	for( int i=0; i<wads.count; i++ ) {
		Wad *w = wads[i];
		reply->putI( ZTmpStr("%d/jobId",i), w->jobId );
		reply->putS( ZTmpStr("%d/wadName",i), w->wadName );

		unsigned int size;
		char *temp = zHashTablePack( w->wad, &size );
		free( temp );
		reply->putI( ZTmpStr("%d/wadSize",i), size );
	}

	reply->putI( "toRemote", id );
	zMsgQueue( reply );
}


// @TODO: I need a cleaner way of doign this so that
// the server console uses the same interface, maybe it
// just opens a connection to itself?


ZMSG_HANDLER( Command ) {
	int i;

	ZTimeUTC now = zTimeUTC();

	if( zmsgIs( cmd, conn ) ) {
		for( i=0; i<ZMsgZocket::listCount; i++ ) {
			ZMsgZocket *z = ZMsgZocket::list[i];
			if( z->isListening() ) {
				printf( "%d: zid=%d Listening\n", i, z->id );
			}
			else {
				Connection *c = connectionFind( z->id );
				if( c ) {
					ZocketAddress zAddr = z->getRemoteAddress();
					printf( "%d: zid=%d addr=%s\n", i, z->id, zAddr.encode() );
					printf( "  connectTime (secs ago)=%d\n", now - c->connectTime );
					printf( "  availForWork=%d\n", c->availForWork );
					printf( "  monitorJobId=%d\n", c->monitorJobId );
					printf( "  loginName=%s\n", c->loginName );
					printf( "  protocols=%s\n", c->protocols );
					printf( "  markedForDelete=%u\n", c->markedForDelete );
				}
				else {
					printf( "%d: zid=%d Orphaned zocket.\n", i, z->id );
				}
			}
		}

		// @TODO: Somehow I want to check that the connections list is totally account for
		// So I need to stick things into a hash and check that everybody is accounted for
	}
	else if( zmsgIs( cmd, jobs ) ) {
		for( i=0; i<jobs.count; i++ ) {
			Job *j = jobs[i];
			printf( "Job %d: jobId=%d\n", i, j->jobId );
			printf( "  protocol=%s\n", j->protocol );
			printf( "  workOrdersRequested=%d\n", j->workOrdersRequested );
			printf( "  workOrdersSent=%d\n", j->workOrdersSent );
			printf( "  workOrdersCompleted=%d\n", j->workOrdersCompleted );
			printf( "  workOrdersDelivered=%d\n", j->workOrdersDelivered );
			printf( "  totalComputationTime=%u\n", j->totalComputationTime );
			printf( "  whenCreated (secs ago)=%d\n", now - j->whenCreated );
			printf( "  creator=%s\n", j->creator );
		}
	}
	else if( zmsgIs( cmd, work ) ) {
		for( i=0; i<workOrders.count; i++ ) {
			WorkOrder *w = workOrders[i];
			printf( "WO %d\n", i );
			printf( "  instruct recs=%d\n", w->instructions.activeCount() );
			printf( "  results recs=%d\n", w->results.activeCount() );
			printf( "  jobId=%d\n", w->jobId );
			printf( "  workOrderId=%d\n", w->workOrderId );
			printf( "  protocol=%s\n", w->protocol );
			printf( "  whenRequested=%u\n", w->whenRequested );
			printf( "  whenSent=%u\n", w->whenSent );
			printf( "  whenCompleted=%u\n", w->whenCompleted );
			printf( "  whenDelivered=%u\n", w->whenDelivered );
			printf( "  assumeAbandonedAfterSeconds=%d\n", w->assumeAbandonedAfterSeconds );
			printf( "  sentUniqId=%d\n", w->sentUniqId );
			printf( "  sentWhoToCredit=%s\n", w->sentWhoToCredit );
		}
	}
	else if( zmsgIs( cmd, acts ) ) {
		for( i=0; i<accounts.count; i++ ) {
			Account *a = accounts[i];
			printf( "Account %d\n", i );
			printf( "  loginName=%s\n", a->loginName );
			printf( "  password=%s\n", a->password );
			printf( "  workOrdersRequested=%d\n", a->workOrdersRequested );
			printf( "  workOrdersCompleted=%d\n", a->workOrdersCompleted );
			printf( "  activeWorkOrderCount=%d\n", a->activeWorkOrderCount );
		}
	}
	else if( zmsgIs( cmd, wads ) ) {
		for( i=0; i<wads.count; i++ ) {
			Wad *w = wads[i];
			printf( "Wad %d\n", i );
			printf( "  jobId=%d\n", w->jobId );
			printf( "  wadName=%s\n", w->wadName );
		}
	}
	else {
		printf(
			"Commands:\n"
			"  conn: show list of current connections.\n"
			"  jobs: show list of current jobs.\n"
			"  work: show list of work orders.\n"
			"  acts: show list of accounts.\n"
			"  wads: show list of accounts.\n"
		);
	}
}

// update
//-----------------------------------------------------------------------------------------

void update() {
	int i;

	// ACCUMUALTE contribution statistics for those accounts that have open work orders
	int weightingSum = 0;
	for( i=0; i<accounts.count; i++ ) {
		if( accounts[i]->activeWorkOrderCount > 0 ) {
			weightingSum += accounts[i]->workOrdersCompleted;
		}
	}

	ZTimeUTC now = zTimeUTC();

	// ASSIGN workorders to available workers
	for( int ci=0; ci<connections.count; ci++ ) {
		Connection *c = connections[ci];
		assert( c );
			// The list should never be sprase

		if( jobs.count > 0 && c->availForWork ) {

			// PICK a random account according to weighting statistics
			int r = zrandI( 0, weightingSum );

			int sum = 0;
			for( i=0; i<accounts.count; i++ ) {
				if( accounts[i]->activeWorkOrderCount > 0 ) {
					sum += accounts[i]->workOrdersCompleted;
					if( sum > r ) {
						break;
					}
				}
			}

			Account *lotteryWinner = accounts[i];

			// LOOP through the jobs starting at a random place to distriubute scheduling looking
			// for a work order that belongs to a job owned by the lotteryWinner
			int found = 0;
			int jobRand = zrandI(0,jobs.count);
			for( int ji=0; !found && ji<jobs.count; ji++ ) {
				Job *j = jobs[ (jobRand+ji) % jobs.count ];

				if( strcmp(j->creator,lotteryWinner->loginName) ) {
					// SKIP jobs which are not owned by the winner of the scheduling lottery
					continue;
				}

				if( j->workOrdersRequested - j->workOrdersCompleted <= 0 ) {
					// SKIP jobs which have all of their work orders completed
					continue;
				}

				// FIND a work order belonging to this job
				for( int wi=0; wi<workOrders.count; wi++ ) {
					WorkOrder *w = workOrders[wi];

					if( w->jobId == j->jobId && strstr(workOrders[wi]->protocol, c->protocols) ) {
						// This workorder is for the job and the worker supports the protocol

						// SEND the work order if it has never been sent or if it has been abandoned
						if(
							w->whenSent == 0.0
							|| (
								now - w->whenSent > (ZTimeUTC)w->assumeAbandonedAfterSeconds
								&& w->whenCompleted == 0
							)
						) {
							j->workOrdersSent++;
							c->availForWork = 0;
							
							static int workSentOrderCounter = 1;
							w->sentUniqId = ++workSentOrderCounter;
							strcpy( w->sentWhoToCredit, c->loginName );
							w->whenSent = zTimeUTC();

							ZMsg *newMsg = new ZMsg;
							newMsg->copyFrom( w->instructions );
							newMsg->putS( "type", "WorkOrderBegin" );
							newMsg->putI( "toRemote", c->zocketId );
							newMsg->putI( "workOrderUniqId", w->sentUniqId );

							zMsgQueue( newMsg );

							saveAll();

							found = 1;
							break;
						}
					}									  
				}
			}
		}

		// GARBAGE collect any connections that are either marked for delete
		// or are connected but who are not marked as avilable for work within 30 seconds
		if( (c->markedForDelete && now > c->markedForDelete) /*|| (now > c->connectTime+10 && !c->loginName[0])*/ ) {
			trace( "Dropped %d\n", c->zocketId );
			int zid = c->zocketId;
			ZMsgZocket::dropId( zid );
			connectionDelete( zid );
			ci--;
				// The list just got one shorter
		}
	}
}


// main
//-----------------------------------------------------------------------------------------

int main( int argc, char **argv ) {
	ZHashTable options;

//ZHashTable test;
//test.putS( "type", "Login" );
//zHashTablePack( test );

	// LOAD options
	zConfigLoadFile( "diwos.cfg", options );
	zCmdParseCommandLine( argc, argv, options );
		// cmdline may override .cfg files

	loadAll();

	int listenPort = options.getI("--port",59997);
	if( listenPort ) {
		trace( "Server listening on port: %d\n", listenPort );

		ZMsgZocket test;

		// OPEN the listening zocket
		ZMsgZocket *server = new ZMsgZocket( ZTmpStr("tcp://*:%d", listenPort), ZO_LISTENING );
		assert( server && server->isOpen() );
	}

	zMsgSetHandler( "default", ZMsgZocket::dispatch );

	int cmdLen = 0;
	char cmd[1024] = {0,};

	zconsoleCharMode();

	while( 1 ) {
		zMsgDispatch();

		ZMsgZocket::wait( 1000, 1 );
		ZMsgZocket::readList();
		update();

		if( zconsoleKbhit() ) {
			char c = zconsoleGetch();

			if( cmdLen < sizeof(cmd)-1 ) {
				if( c == '\r' || c == '\n' ) {
					cmdLen = 0;

					ZMsg *z = new ZMsg;
					z->putS( "type", "Command" );
					z->putS( "cmd", cmd );
					zMsgQueue( z );

					printf( "\n" );
				}
				else {
					cmd[cmdLen++] = c;
					cmd[cmdLen] = 0;
				}
			}
		}
	}

	return 0;
}


