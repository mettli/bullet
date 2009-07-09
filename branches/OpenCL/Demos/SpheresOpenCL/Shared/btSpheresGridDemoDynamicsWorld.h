/*
Bullet Continuous Collision Detection and Physics Library, http://bulletphysics.org
Copyright (C) 2006 - 2009 Sony Computer Entertainment Inc. 

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#ifndef BT_SPHERES_GRID_DEMO_DYNAMICS_WORLD_H
#define BT_SPHERES_GRID_DEMO_DYNAMICS_WORLD_H

// standard utility and system includes
#include <CL/cl.h>
// Extra CL/GL include
#include <CL/cl_gl.h>

// check OpenCL version
#if (defined(CL_PLATFORM_MINI_CL) || defined(CL_PLATFORM_NVIDIA))
	// OK
#else
	#error ERROR : Sorry, this version of OpenCL is not supported yet
#endif


#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h"
#include "BulletDynamics/ConstraintSolver/btTypedConstraint.h"
#include "BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h"

#include "btSpheresGridDemoSharedDefs.h"
#include "btSpheresGridDemoSharedTypes.h"




#define SPHERES_GRID_MAX_OBJS (65536)
#define SPHERES_GRID_MAX_NEIGHBORS (32)


struct btInt2
{
	int x;
	int y;
};

struct btPairId
{
	int m_objA;
	int m_objB;
	int m_sphA;
	int m_sphB;
	int m_batch;
	int m_pair;
	int m_pad[2];
	void quickSort(btPairId* pData, int lo, int hi)
	{
		if(lo >= hi) return;
		int i=lo, j=hi;
		btPairId x = pData[(lo+hi)/2];
		do
		{    
			while(pData[i].m_batch < x.m_batch) i++; 
			while(x.m_batch < pData[j].m_batch) j--;
			if(i <= j)
			{
				btPairId t = pData[i];
				pData[i] = pData[j];
				pData[j] = t;
				i++; j--;
			}
		} while(i <= j);
		if(lo < j) pData->quickSort(pData, lo, j);
		if(i < hi) pData->quickSort(pData, i, hi);
	}
};


struct btSpheresContPair
{
	btVector3 m_contact; // + penetration in w
	btVector3 m_normal;  // + impulse accumulator in w
};

class btSpheresGridDemoDynamicsWorld : public btDiscreteDynamicsWorld
{
protected:
	int			m_numSpheres;
	int			m_numObjects;
	int			m_hashSize; // power of 2 >= m_numSpheres;
	int			m_numGridCells; 
	int			m_maxNeighbors;
	int			m_numPairs; 
	int			m_maxPairs;
	int			m_numBatches;
	int			m_maxBatches;
	// CPU side data
	btAlignedObjectArray<btVector3>	m_hShapeBuf;
	btAlignedObjectArray<btInt2>	m_hShapeIds; // per each body : (start index, num_spheres)
	btAlignedObjectArray<int>		m_hBodyIds;  // per each sphere : parent body index
	btAlignedObjectArray<btVector3>	m_hPos;
	btAlignedObjectArray<btVector3>	m_hTrans;
	btAlignedObjectArray<btVector3>	m_hLinVel;
	btAlignedObjectArray<btVector3>	m_hAngVel;
	btAlignedObjectArray<btVector3>	m_hInvInertiaMass;
	btAlignedObjectArray<btInt2>	m_hPosHash;
	btAlignedObjectArray<int>		m_hCellStart;
	btAlignedObjectArray<int>		m_hPairBuff;
	btAlignedObjectArray<btInt2>	m_hPairBuffStartCurr;
	btAlignedObjectArray<int>		m_hPairScan;
	btAlignedObjectArray<btPairId>	m_hPairIds;
	btAlignedObjectArray<int>		m_hObjUsed;
	btAlignedObjectArray<int>		m_hNumPairsInBatch;
	btAlignedObjectArray<btSpheresContPair> m_hContacts;
	// GPU side data
	cl_mem		m_dShapeBuf;
	cl_mem		m_dShapeIds;
	cl_mem		m_dBodyIds;
	cl_mem		m_dPos;
	cl_mem		m_dTrans;
	cl_mem		m_dLinVel;
	cl_mem		m_dAngVel;
	cl_mem		m_dInvInertiaMass;
	cl_mem		m_dPosHashSrc;
	cl_mem		m_dPosHashDst;
	cl_mem		m_dCellStart;
	cl_mem		m_dPairBuff;
	cl_mem		m_dPairBuffStartCurr;
	cl_mem		m_dPairScan;
	cl_mem		m_dPairIds;
	cl_mem		m_dObjUsed;
	cl_mem		m_dContacts;
	cl_mem		m_dSimParams; // copy of m_simParams : global simulation paramerers such as gravity, etc. 

	bool			m_useCPU;

	// OpenCL 
	cl_context			m_cxMainContext;
	cl_device_id		m_cdDevice;
	cl_command_queue	m_cqCommandQue;
	cl_program			m_cpProgram;
	cl_kernel			m_ckSetSpheresKernel;
	cl_kernel			m_ckPredictUnconstrainedMotionKernel;
	cl_kernel			m_ckIntegrateTransformsKernel;
	cl_kernel			m_ckBroadphaseCDKernel;
	cl_kernel			m_ckSetupContactsKernel;
	cl_kernel			m_ckSolveConstraintsKernel;

	btVector3			m_worldMin;
	btVector3			m_worldMax;
	btScalar			m_minSphereRad;
	btScalar			m_maxSphereRad;
	btVector3			m_cellSize;

public:
	// vbo variables
	GLuint			m_vbo;
	unsigned int	m_posVbo;
	unsigned int	m_colVbo;
	btSimParams		m_simParams;
	float			m_timeStep;
	int				m_pairOffset;

	int getNumSpheres() { return m_hShapeBuf.size(); }
	float* getPosBuffer() { return (float*)&(m_hPos[0]); }


	btSpheresGridDemoDynamicsWorld(btDispatcher* dispatcher,btBroadphaseInterface* pairCache,btConstraintSolver* constraintSolver,btCollisionConfiguration* collisionConfiguration,
			int maxObjs = SPHERES_GRID_MAX_OBJS, int maxNeighbors = SPHERES_GRID_MAX_NEIGHBORS)
		: btDiscreteDynamicsWorld(dispatcher, pairCache, constraintSolver, collisionConfiguration)
	{ 
		m_useCPU = true;
//		m_useCPU = false;
		m_simParams.m_gravity[0] = 0.f;
		m_simParams.m_gravity[1] = -10.f;
		m_simParams.m_gravity[2] = 0.f;
		m_simParams.m_gravity[3] = 0.f;
	}
	virtual ~btSpheresGridDemoDynamicsWorld();
	virtual int	stepSimulation( btScalar timeStep,int maxSubSteps=1, btScalar fixedTimeStep=btScalar(1.)/btScalar(60.));

	void initDeviceData();
	void initCLKernels(int argc, char** argv);
	void createVBO();
	void postInitDeviceData();
	void getShapeData();
	void allocateBuffers();
	void grabSimulationData();
	void adjustGrid();
	void setPointers();
	void runSetSpheresKernel();
	void runPredictUnconstrainedMotionKernel();
	void runIntegrateTransformsKernel();
	void runSortHashKernel();
	void runFindCellStartKernel();
	void runBroadphaseCDKernel();
	void runScanPairsKernel();
	void runCompactPairsKernel();
	void runSetupBatchesKernel();
	void runSortBatchesKernel();
	void runSetupContactsKernel();
	void runSolveConstraintsKernel();
	void solvePairCPU(btSpheresContPair* pPair, int pairIdx);
};


#endif //BT_SPHERES_GRID_DEMO_DYNAMICS_WORLD_H
