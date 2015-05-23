/*
 * Copyright 2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "openvdb.h"
#include "scene.h"
#include "util_logging.h"
#include "util_progress.h"

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENVDB

#define MAX_VOLUME 1024

VolumeManager::VolumeManager()
{
	openvdb::initialize();

	scalar_grids.reserve(64);
	vector_grids.reserve(64);
	current_grids.reserve(64);
	float_volumes.reserve(64);
	float3_volumes.reserve(64);

	need_update = true;
}

VolumeManager::~VolumeManager()
{
	scalar_grids.clear();
	vector_grids.clear();
	current_grids.clear();
	float_volumes.clear();
	float3_volumes.clear();
}

static inline void catch_exceptions()
{
	try {
		throw;
	}
	catch (const openvdb::IoError& e) {
		std::cerr << e.what() << "\n";
	}
}

int VolumeManager::add_volume(const string& filename, const string& name, int sampling, int grid_type)
{
	using namespace openvdb;
	size_t slot = -1;

	if((slot = find_existing_slot(filename, name, sampling, grid_type)) != -1) {
		return slot;
	}

	try {
		io::File file(filename);
		file.open();

		if(grid_type == NODE_VDB_FLOAT) {
			FloatGrid::Ptr grid = gridPtrCast<FloatGrid>(file.readGrid(name));
			slot = add_scalar_grid(grid);
		}
		else if(grid_type == NODE_VDB_FLOAT3) {
			Vec3SGrid::Ptr grid = gridPtrCast<Vec3SGrid>(file.readGrid(name));
			slot = add_vector_grid(grid);
		}

		add_grid_description(filename, name, sampling, slot);

		need_update = true;
	}
	catch (...) {
		catch_exceptions();
		need_update = false;
		slot = -1;
	}

	return slot;
}

int VolumeManager::find_existing_slot(const string& filename, const string& name, int sampling, int grid_type)
{
	for(size_t i = 0; i < current_grids.size(); ++i) {
		GridDescription grid = current_grids[i];

		if(grid.filename == filename && grid.name == name) {
			if(grid.sampling == sampling) {
				return grid.slot;
			}
			else {
				/* sampling was changed, remove the sampler */
				if(grid_type == NODE_VDB_FLOAT) {
					delete float_volumes[grid.slot];
					float_volumes[grid.slot] = NULL;
				}
				else {
					delete float3_volumes[grid.slot];
					float3_volumes[grid.slot] = NULL;
				}

				/* remove the grid description too */
				std::swap(current_grids[i], current_grids.back());
				current_grids.pop_back();
				break;
			}
		}
	}

	return -1;
}

template <typename Container>
size_t find_empty_slot(Container container)
{
	size_t slot = 0;

	for(; slot < container.size(); ++slot) {
		if(!container[slot]) {
			break;
		}
	}

	if(slot == container.size()) {
		if(slot == MAX_VOLUME) {
			printf("VolumeManager::add_volume: volume sampler limit reached %d!\n",
			       MAX_VOLUME);
			return -1;
		}

		container.resize(slot + 1);
	}

	return slot;
}

size_t VolumeManager::add_scalar_grid(openvdb::FloatGrid::Ptr grid)
{
	size_t slot = find_empty_slot(float_volumes);

	if(slot == -1) return -1;

	vdb_float_volume *sampler = new vdb_float_volume(grid);

	float_volumes.insert(float_volumes.begin() + slot, sampler);
	scalar_grids.push_back(grid);

	return slot;
}

size_t VolumeManager::add_vector_grid(openvdb::Vec3SGrid::Ptr grid)
{
	size_t slot = find_empty_slot(float3_volumes);

	if(slot == -1) return -1;

	vdb_float3_volume *sampler = new vdb_float3_volume(grid);

	float3_volumes.insert(float3_volumes.begin() + slot, sampler);
	vector_grids.push_back(grid);

	return slot;
}

void VolumeManager::add_grid_description(const string& filename, const string& name, int sampling, int slot)
{
	GridDescription descr;
	descr.filename = filename;
	descr.name = name;
	descr.sampling = sampling;
	descr.slot = slot;

	current_grids.push_back(descr);
}

void VolumeManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress)
{
	(void)scene;

	if(!need_update) {
		return;
	}

	device_free(device, dscene);
	progress.set_status("Updating OpenVDB volumes", "Sending samplers to device.");

	for(size_t i = 0; i < float_volumes.size(); ++i) {
		if(!float_volumes[i]) {
			continue;
		}
		device->const_copy_to("__float_volume", float_volumes[i], i);
	}

	for(size_t i = 0; i < float3_volumes.size(); ++i) {
		if(!float3_volumes[i]) {
			continue;
		}
		device->const_copy_to("__float3_volume", float3_volumes[i], i);
	}

	if(progress.get_cancel()) {
		return;
	}

	VLOG(1) << "Volume samplers allocate: __float_volume, " << float_volumes.size() * sizeof(float_volume) << " bytes";
	VLOG(1) << "Volume samplers allocate: __float3_volume, " << float3_volumes.size() * sizeof(float3_volume) << " bytes";

	for(size_t i = 0; i < scalar_grids.size(); ++i) {
		VLOG(1) << scalar_grids[i]->getName() << " memory usage: " << scalar_grids[i]->memUsage() / 1024.0f << " kilobytes.\n";
	}

	for(size_t i = 0; i < vector_grids.size(); ++i) {
		VLOG(1) << vector_grids[i]->getName() << " memory usage: " << vector_grids[i]->memUsage() / 1024.0f << " kilobytes.\n";
	}

	need_update = false;
}

void VolumeManager::device_free(Device */*device*/, DeviceScene */*dscene*/)
{
}

#else

VolumeManager::VolumeManager()
{
	need_update = false;
}

VolumeManager::~VolumeManager()
{
}

int VolumeManager::add_volume(const string& /*filename*/, const string& /*name*/, int /*sampling*/, int /*grid_type*/)
{
	return -1;
}

void VolumeManager::device_update(Device */*device*/, DeviceScene */*dscene*/, Scene */*scene*/, Progress& /*progress*/)
{
}

void VolumeManager::device_free(Device */*device*/, DeviceScene */*dscene*/)
{
}

#endif

CCL_NAMESPACE_END