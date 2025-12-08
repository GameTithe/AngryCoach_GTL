#pragma once
 
#include "NvCloth/include/Factory.h"
#include "NvCloth/include/Fabric.h"
#include "NvCloth/include/Solver.h"
#include "NvCloth/include/Cloth.h"
#include "NvCloth/include/Callbacks.h"
#include "NvCloth/include/NvClothExt/ClothFabricCooker.h"
#include "physx/include/foundation/PxAllocatorCallback.h"
#include "physx/include/foundation/PxErrorCallback.h"

class FClothManager
{
public:

	static FClothManager& GetInstance()
	{
		static FClothManager Instance;

		if (!Instance.bInit)
		{
			Instance.Initialize();
		}
		return Instance;
	}
	 
	void Initialize();
	void Shutdown();

	void CreateSolver();
	void CreateFactory();
	void ClothSimulation(float DeltaSeconds);
	 
	void AddClothToSolver(nv::cloth::Cloth* Cloth);

	nv::cloth::Factory* GetFactory() { return factory; }
	nv::cloth::Solver* GetSolver() { return solver; }

protected:
	nv::cloth::Factory* factory = nullptr;
	nv::cloth::Solver* solver = nullptr;

	bool bInit = false; 
};