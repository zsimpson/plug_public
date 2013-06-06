// @ZBS {
//		+DESCRIPTION {
//			Distributed Work Order Tester Client
//		}
//		*MODULE_DEPENDS diwo.cpp
//		*INTERFACE console
//		*SDK_DEPENDS pthread
// }

// OPERATING SYSTEM specific includes:
#ifdef WIN32
	#include "io.h"
#else
	#include "unistd.h"
	#include "errno.h"
#endif
// SDK includes:
#include "pthread.h"
// STDLIB includes:
#include "assert.h"
#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "fcntl.h"
#include "sys/stat.h"
// MODULE includes:
// ZBSLIB includes:
#include "zmsg.h"
#include "zplatform.h"
#include "zmsgzocket.h"
#include "ztime.h"
#include "zconfig.h"
#include "zcmdparse.h"
#include "ztl.h"
#include "ztmpstr.h"
#include "zfilelock.h"
#include "zwildcard.h"


void trace( char *fmt, ... ) {
	va_list argptr;
	va_start( argptr, fmt );
	vfprintf( stdout, fmt, argptr );
	va_end( argptr );
	fflush( stdout );
}

ZHashTable wads;
	// A hash of hashes

ZHashTable *wadCacheLoad( int jobId, char *wadName ) {
	ZTmpStr filename( "wadcache/%d-%s", jobId, wadName );

	ZHashTable *h = (ZHashTable *)wads.getp( filename );
	if( h ) {
		return h;
	}

	FILE *file = fopen( filename, "rb" );
	while( file ) {
		int locked = zFileLock( fileno(file), ZFILELOCK_EXCLUSIVE );
		if( locked ) {
			struct stat s;
			stat( filename, &s );
			int len = s.st_size;

			if( len == 0 ) {

				// If this gets called after then open below but before the lock
				// which I'm making more possible by adding a sleep in the below

				trace( "***Zero scenario, trying again.  jobId=%d wadName=%s\n", jobId, wadName );
				zFileUnlock( fileno(file) );
				continue;
			}

			char *buf = (char *)malloc( len );
			int read = fread( buf, len, 1, file );
			zFileUnlock( fileno(file) );

			if( read == 1 ) {
				ZHashTable *wad = new ZHashTable;
				zHashTableUnpack( buf, *wad );
				wads.putP( filename, wad );
				fclose( file );
				return wad;
			}
		}
		fclose( file );
		file = 0;
	}
	return 0;
}

int wadCacheCreate( int jobId, char *wadName, ZHashTable *wad ) {
	ZTmpStr filename( "wadcache/%d-%s", jobId, wadName );

	// @TODO: add code for making the directory if it doesn't exist
	//trace( "Cache create open. %s\n", (char*)filename );
	#ifndef O_BINARY
		#define O_BINARY 0
	#endif
	int fd = open( filename, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0644 );
	if( fd < 0 ) {
		// It can occur that multiple processes get this message simultaneously
		// so it must be ensured that they can not create the file simultaneously
		trace( "open fail jobId=%d wadName=%s errno=%d\n", jobId, wadName, errno );
		return 0;
	}
	//trace( "open success. %s\n", (char*)filename );

	//zTimeSleepSecs( 2 );

	//trace( "fdopen.... %s\n", (char*)filename );

	FILE *file = fdopen( fd, "wb" );
	assert( file );

	if( file ) {
		//trace( "fdopen success. %s\n", (char*)filename );
		int locked = zFileLock( fileno(file), ZFILELOCK_EXCLUSIVE );
		if( locked ) {
		//trace( "lock success. %s\n", (char*)filename );

			wads.putP( filename, wad );

			unsigned int dataLen;
			char *data = zHashTablePack( *wad, &dataLen );
			int write = fwrite( data, dataLen, 1, file );
			if( write != 1 ) {
				trace( "Write fail. dataLen=%d errno=%d\n", dataLen, errno );
			}

			struct stat s;
//			stat( filename, &s );
//			trace( "1. jobId = %d, wadName=%s, dataLen = %d, statesize=%d\n", jobId, wadName, dataLen, s.st_size );
//			assert( s.st_size > 0 && s.st_size == dataLen );

//			int unlocked = zFileUnlock( fileno(file) );
//			assert( unlocked );
			fclose( file );
			free( data );

			// CHECK for the zero scenario nwhich shouldn't happen now that we have exclusive open
			stat( filename, &s );
			trace( "2. jobId = %d, wadName=%s, dataLen = %d, statesize=%d\n", jobId, wadName, dataLen, s.st_size );
			assert( s.st_size > 0 && (unsigned int)s.st_size == dataLen );

			return 1;
		}
		fclose( file );

		struct stat s;
		stat( filename, &s );
		assert( s.st_size > 0 );
	}

	assert( 0 );
		// Shouldn't ever fail unless the directory doesn't exist
		// @TODO: add directory making code
	return 0;
}

void wadCacheAccumJobList( ZMsg *list ) {
	ZWildcard w( "wadcache/*" );
	while( w.next() ) {
		char temp[128];
		strcpy( temp, w.getName() );
		char *dash = strchr( temp, '-' );
		if( dash ) {
			*dash = 0;
			list->putI( temp, 1 );
		}
	}
}

void wadCacheDelete( int jobId ) {
return;
	ZWildcard w( "wadcache/*" );
	while( w.next() ) {
		char temp[128];
		strcpy( temp, w.getName() );
		char *dash = strchr( temp, '-' );
		if( dash ) {
			*dash = 0;
			int wJobId = atoi( temp );
			if( wJobId == jobId ) {
				unlink( w.getFullName() );
			}
		}
	}
}

char *address = 0;

void *workerThreadMain( void *args ) {
	int i;

	// OPEN a connection to the work order server

	ZMsgZocket zocket;
	zocket.setSendOnConnect( "" );
	zocket.setSendOnDisconnect( "" );

	while( 1 ) {
	
		if( ! zocket.isOpen() ) {
			trace( "Opening %s\n", address );
			zocket.open( address, ZO_EXCLUDE_FROM_POLL_LIST );
		}

		if( zocket.isOpen() ) {
			ZMsg login;
			login.putS( "type", "Login" );
			login.putS( "loginName", "tester1" );
			login.putS( "password", "password" );
			zocket.write( &login );

			ZMsg *loginReply = 0;
			zocket.read( &loginReply, 1 );
			if( loginReply && ! loginReply->isS( "type", "LoginSuccess" ) ) {
				delete loginReply;
				break;
			}
			delete loginReply;

			ZMsg jobDeadListRequest;
			jobDeadListRequest.putS( "type", "JobDeadListRequest" );
			wadCacheAccumJobList( &jobDeadListRequest );
			zocket.write( &jobDeadListRequest );

			ZMsg *deadListReply = 0;
			zocket.read( &deadListReply, 1 );
			if( deadListReply && deadListReply->isS( "type", "JobDeadListReply" ) ) {
				for( ZHashWalk n( *deadListReply ); n.next(); ) {
					int jobId = atoi( n.key );
					if( jobId ) {
						wadCacheDelete( jobId );
					}
				}
			}

			ZMsg avail;
			avail.putS( "type", "AvailableForWork" );
			avail.putS( "protocols", "*testproc" );
			zocket.write( &avail );

			trace( "Read\n" );

			ZMsg *workOrder = 0;
			zocket.read( &workOrder, 1 );

			if( workOrder && workOrder->isS("type","WorkOrderBegin") ) {

				// LOAD all of the required wads
				int jobId = workOrder->getI("jobId");
				int wadCount = workOrder->getI( "wadCount" );
				for( i=0; i<wadCount; i++ ) {
					char *wadName = workOrder->getS( ZTmpStr("wadName-%d",i) );

					int loaded = 0;
					while( ! loaded ) {

						ZHashTable *w = wadCacheLoad( jobId, wadName );
						if( ! w ) {
							// The wad is not available locally, request it
							ZMsg wadRequest;
							wadRequest.putS( "type", "WadRequest" );
							wadRequest.putI( "jobId", jobId );
							wadRequest.putS( "wadName", wadName );
							zocket.write( &wadRequest );

							ZMsg *wadReply = 0;
							zocket.read( &wadReply, 1 );
							if( wadReply && wadReply->isS("type","WadReply") ) {
								loaded = wadCacheCreate( jobId, wadName, wadReply );
							}
						}
						else {
							loaded = 1;
						}
					}
				}

				// DROP the connection while doing the work
				zocket.close();

				// DO the work
				// @TEMP: This sleep simulates the work that would be done
				trace( "Doing workOrder jobId=%d workOrderId=%d uniqId=%d\n", workOrder->getI("jobId"), workOrder->getI("workOrderId"), workOrder->getI("workOrderUniqId") );

				// PRINT the wad contents
				wadCount = workOrder->getI( "wadCount" );
				for( i=0; i<wadCount; i++ ) {
					char *wadName = workOrder->getS( ZTmpStr("wadName-%d",i) );
					ZHashTable *w = wadCacheLoad( jobId, wadName );
					if( !w ) {
						// I've seen this assert hit.  Seems like it can only
						// happen if one of the other processes has deleted it
					}
					assert( w );
					trace( "wad %d: value=%d\n", i, w->getI("value") );
					assert( w->getI("value") == i );
				}

				// SIMULATE doing some work for a few seconds
//while(1)
				zTimeSleepSecs( 3 );


				// WRITE results into completeMsg
				ZMsg woComplete;
				
				// @TEMP: Placeholder for soing real math return an incrementing number
				static int counter = 1;
				woComplete.putI( "result", counter++ );

				// RECONNECT and submit the work
				zocket.open( address, ZO_EXCLUDE_FROM_POLL_LIST );
				if( zocket.isOpen() ) {

					woComplete.putS( "type", "WorkOrderComplete" );
					woComplete.putI( "workOrderUniqId", workOrder->getI( "workOrderUniqId" ) );

					trace( "Sending workOrder jobId=%d workOrderId=%d uniqId=%d\n", workOrder->getI("jobId"), workOrder->getI("workOrderId"), workOrder->getI("workOrderUniqId") );

					zocket.write( &woComplete );

					// Note, we are not closing the zocket here so that when 
					// we return to the top of the loop it will find that it
					// is still open and send a new "LoginAvailForWork"
				}
			}

			delete workOrder;
		}

		zTimeSleepSecs( 1 );
	}

	return 0;
}

struct TestJob {
	char filename[256];
	ZTLVec<ZHashTable *> workOrders;

	TestJob( char *_filename ) {
		if( !_filename || !*_filename ) {
			_filename = ZTmpStr( "test.%d.job", rand() % 10000 );
		}
		strcpy( filename, _filename );
		trace( "Job name %s\n", filename );
	}

	void create();
	void save();
	int load();
};

void TestJob::create() {
	// CREATE some bogus work
	for( int i=0; i<30; i++ ) {
		ZHashTable *order = new ZHashTable;
		order->putS( "type", "WorkOrderCreate" );
		order->putI( "workOrderId", i );
		order->putI( "assumeAbandonedAfterSeconds", 30 );
		order->putI( "randomKey", rand() );
		order->putI( "result", 0 );
		workOrders.add( order );
	}
}

void TestJob::save() {
	FILE *file = fopen( filename, "wb" );
	for( int i=0; i<workOrders.count; i++ ) {
		unsigned int size = 0;
		char *buf = zHashTablePack( *workOrders[i], &size );
		fwrite( buf, size, 1, file );
		free( buf );
	}
	fclose( file );
}

int TestJob::load() {
	FILE *file = fopen( filename, "rb" );
	if( !file ) {
		return 0;
	}
	fseek( file, 0, SEEK_END );
	int fileLen = ftell( file );
	fseek( file, 0, SEEK_SET );
	char *buf = (char *)malloc( fileLen );
	char *end = buf + fileLen;
	fread( buf, fileLen, 1, file );
	fclose( file );

	while( buf < end ) {
		ZHashTable *order = new ZHashTable;
		int len = zHashTableUnpack( buf, *order );
		workOrders.add( order );
		buf += len;
	}

	return 1;
}

void *requesterThreadMain( void *args ) {
	int i;

	TestJob *job = (TestJob *)args;

	ZMsgZocket zocket;
	zocket.setSendOnConnect( "" );
	zocket.setSendOnDisconnect( "" );

	int jobId = 0;
	int deliveredCount = 0;
	while( deliveredCount < job->workOrders.count ) {
		if( ! zocket.isOpen() ) {
			trace( "Attempting to open %s\n", address );

			zocket.open( address, ZO_EXCLUDE_FROM_POLL_LIST );

			if( zocket.isOpen() ) {
				trace( "Opened\n" );

				// LOGIN
				ZMsg login;
				login.putS( "type", "Login" );
				login.putS( "loginName", "tester1" );
				login.putS( "password", "password" );
				zocket.write( &login );

				// Reply...
				ZMsg *loginReply = 0;
				zocket.read( &loginReply, 1 );
				if( loginReply && ! loginReply->isS( "type", "LoginSuccess" ) ) {
					delete loginReply;
					return 0;
				}
				delete loginReply;

				deliveredCount = 0;

				// CREATE job on server if it doesn't exist
				jobId = job->workOrders[0]->getI( "jobId" );
				if( !jobId ) {
					trace( "Creating new job\n" );

					ZMsg jobCreate;
					jobCreate.putS( "type", "JobCreate" );
					jobCreate.putS( "protocol", "*testproc" );
					zocket.write( &jobCreate );

					// Reply...
					ZMsg *jobCreateReply;
					zocket.read( &jobCreateReply, 1 );
					if( jobCreateReply ) {
						if( jobCreateReply->isS("type","JobCreated") ) {
							jobId = jobCreateReply->getI( "jobId" );
						}
						else {
							trace( "JobCreate failed reason=%s\n", jobCreateReply->getS("reason","") );
							delete jobCreateReply;
							return 0;
						}
						delete jobCreateReply;
					}

					// UPLOAD wads
					for( i=0; i<2; i++ ) {
						ZMsg wadCreate;
						wadCreate.putS( "type", "WadCreate" );
						wadCreate.putI( "jobId", jobId );
						wadCreate.putS( "wadName", ZTmpStr("wad%d",i) );
						wadCreate.putI( "value", i );
						zocket.write( &wadCreate );
					}

					// CREATE work orders
					for( i=0; i<job->workOrders.count; i++ ) {
						job->workOrders[i]->putI( "jobId", jobId );

						// REFERENCE all the wads in every workorder
						job->workOrders[i]->putI( "wadCount", 2 );
						for( int j=0; j<job->workOrders.count; j++ ) {
							ZTmpStr wadName( "wad%d", j );
							job->workOrders[i]->putS( ZTmpStr("wadName-%d",j), wadName );
						}

						zocket.write( job->workOrders[i] );
					}
					job->save();
				}
				else {
					trace( "Requesting existing job status\n" );

					// REQUEST that we are informed about all new work orders on this job
					ZMsg jobMonitor;
					jobMonitor.putS( "type", "JobMonitor" );
					jobMonitor.putI( "jobId", jobId );
					zocket.write( &jobMonitor );

					// FETCH the status
					ZMsg jobStatusReq;
					jobStatusReq.putS( "type", "JobStatusRequest" );
					jobStatusReq.putI( "jobId", jobId );
					zocket.write( &jobStatusReq );

					ZMsg *jobStatus;
					zocket.read( &jobStatus, 1 );

					if( jobStatus && jobStatus->isS("type","JobStatusReply") ) {
						int requestCount = jobStatus->getI( "workOrdersRequested" );

						assert( !requestCount || requestCount == job->workOrders.count );

						// FETCH all of the completed work orders that we don't have locally
						for( int i=0; i<job->workOrders.count; i++ ) {
							int whenCompleted = jobStatus->getI( ZTmpStr("wo-%d-whenCompleted",i) );

							if( job->workOrders[i]->getI("whenDelivered") ) {
								// We already have the work order locally
								deliveredCount++;
							}
							else if( whenCompleted > 0 ) {
								// The work order has been completed on the server
								// but we don't have it locally so we request it
								ZMsg woDeliverReq;
								woDeliverReq.putS( "type", "WorkOrderDeliverRequest" );
								woDeliverReq.putI( "jobId", jobId );
								woDeliverReq.putI( "workOrderId", i );
								zocket.write( &woDeliverReq );
							}
						}
					}
				}
			}
		}
		else {
			// ZOCKET is open, attempt normal communication
			ZMsg *woAvail;
			zocket.read( &woAvail, 1 );

			if( woAvail ) {
				if( woAvail->isS( "type", "WorkOrderAvailable" ) ) {
					int jobId = woAvail->getI( "jobId" );
					int workOrderId = woAvail->getI( "workOrderId" );

					ZMsg woDeliverReq;
					woDeliverReq.putS( "type", "WorkOrderDeliverRequest" );
					woDeliverReq.putI( "jobId", jobId );
					woDeliverReq.putI( "workOrderId", workOrderId );
					zocket.write( &woDeliverReq );
				}
				else if( woAvail->isS( "type", "WorkOrderDeliver" ) ) {
					int availJobId = woAvail->getI("jobId");
					int availWorkOrderId = woAvail->getI("workOrderId");
					assert( availJobId == jobId );
					assert( availWorkOrderId >= 0 && availWorkOrderId < job->workOrders.count );

					trace( "WorkOrderDeliver %d %d. (%d of %d complete)\n", availJobId, availWorkOrderId, deliveredCount, job->workOrders.count );
					job->workOrders[availWorkOrderId]->copyFrom( *woAvail );
					job->save();
					deliveredCount++;
				}
				else if( woAvail->isS( "type", "WorkOrderDeliverFail" ) ) {
					trace( "WorkOrderDeliverFail %d %d\n", woAvail->getI("jobId"), woAvail->getI("workOrderId") );
				}

				delete woAvail;
			}
		}
	}

	// JOB is complete
	trace( "Job complete\n" );

	ZMsg jobDel;
	jobDel.putS( "type", "JobDelete" );
	jobDel.putI( "jobId", jobId );
	zocket.write( &jobDel );


	// WAIT for acknowledge before terminating the zocket
	ZMsg *jobDelAck;
	zocket.read( &jobDelAck, 1 );
	if( jobDelAck ) {
		assert( jobDelAck->isS( "type", "JobDeleteAck" ) );
	}

	return 0;
}



void *lockTestThreadMain( void *args ) {
	char *filename = "test.dat";

	while( 1 ) {

//		int fd = open( filename, O_RDWR | O_CREAT | O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH );
		int fd = open( filename, O_RDWR | O_CREAT | O_EXCL, 0xFFFF );
		if( fd < 0 ) {
			// It can occur that multiple processes get this message simultaneously
			// so it must be ensured that they can not create the file simultaneously
			printf( "create error\n" );
			continue;
		}

		FILE *file = fdopen( fd, "wb" );
		assert( file );

		if( file ) {
			char buf[5] = {0,};

			int locked = zFileLock( fileno(file), ZFILELOCK_EXCLUSIVE );
			if( locked ) {
				printf( "locked\n" );
				int write = fwrite( buf, 5, 1, file );
				assert( write == 1 );
				zFileUnlock( fileno(file) );
				fclose( file );
				close( fd );

				struct stat s;
				stat( filename, &s );
				assert( s.st_size > 0 );
			}
		}
	}
}


int main( int argc, char **argv ) {
	int i;
/*
	for( i=0; i<100000; i++ ) {
		ZTmpStr filename( "wadcache/test%d", i );
		int fd = open( filename, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0xFFFF );
		if( fd < 0 ) {
			// It can occur that multiple processes get this message simultaneously
			// so it must be ensured that they can not create the file simultaneously
			printf( "failed open\n" );
			return 0;
		}

		FILE *file = fdopen( fd, "wb" );
		assert( file );

		if( file ) {
			int locked = zFileLock( fileno(file), ZFILELOCK_EXCLUSIVE );
			if( locked ) {
				printf( "lock %d\n", i );

				int dataLen = 5;
				char *data[5] = {0,};
				int write = fwrite( data, dataLen, 1, file );
				if( write != 1 ) {
					trace( "Write fail. dataLen=%d errno=%d\n", dataLen, errno );
				}

				struct stat s;

				fclose( file );

				stat( filename, &s );
				assert( s.st_size > 0 && s.st_size == dataLen );
			}
		}
	}
*/

	ZHashTable options;

	// LOAD options
	zConfigLoadFile( "diwos.cfg", options );
	zCmdParseCommandLine( argc, argv, options );
		// cmdline may override .cfg files

	pthread_t threadID[2];

	if( options.getS("--address") ) {
		address = strdup( ZTmpStr("tcp://%s",options.getS("--address")) );
	}
	else {
		printf( "Specify --address\n" );
		return 0;
	}

	if( options.getI("--startWorker") ) {

	//	int numProc = zplatformGetNumProcessors();
		int numProc = 1;
		for( i=0; i<numProc; i++ ) {
			pthread_attr_t attr;
			pthread_attr_init( &attr );
			pthread_attr_setstacksize( &attr, 1024*4096 );
			pthread_create( &threadID[i], &attr, &workerThreadMain, 0 );
			pthread_attr_destroy( &attr );
		}

		for( i=0; i<numProc; i++ ) {
			pthread_join( threadID[i], 0 );
		}
	}

	if( options.getI("--startRequester") ) {
		// LOAD the job or create it if it doesn't alrady exist

		char *jobName = options.getS("--jobName");
		TestJob testJob( jobName );
		if( ! testJob.load() ) {
			testJob.create();
			testJob.save();
		}

		pthread_attr_t attr;
		pthread_attr_init( &attr );
		pthread_attr_setstacksize( &attr, 1024*4096 );
		pthread_create( &threadID[0], &attr, &requesterThreadMain, &testJob );
		pthread_attr_destroy( &attr );

		pthread_join( threadID[0], 0 );
	}

	return 0;
}

