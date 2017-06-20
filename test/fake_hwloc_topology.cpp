
#include <array>

#include <hwloc.h>

#include "../src/util/topology.hpp"
#include "test_helper.h"


int hwloc_topology_init(hwloc_topology_t */*topology*/) {
	return 0;
}

int hwloc_topology_load(hwloc_topology_t /*topology*/) {
	return 0;
}
// inline function in hwloc/inline.h
// int hwloc_get_type_or_below_depth(hwloc_topology_t /*topology*/, hwloc_obj_type_t type);
int hwloc_get_type_depth (hwloc_topology_t /*topology*/, hwloc_obj_type_t type) {
	ASSERT_EQ(HWLOC_OBJ_NUMANODE, type);
	return 1;
}
hwloc_obj_t hwloc_get_obj_by_depth (hwloc_topology_t /*topology*/, unsigned depth, unsigned idx) {
	struct LocalData {
		std::array<hwloc_obj, 5> nodes;

		LocalData() {
			// simulate a POWER style architecture
			memset(nodes.data(), 0, sizeof(hwloc_obj) * nodes.size());
			for (unsigned int i = 0; i < nodes.size(); ++i) {
				auto & node = nodes[i];
				node.type = HWLOC_OBJ_NUMANODE;
				node.os_index = i;
				node.cpuset = hwloc_bitmap_alloc();
			}
			// hwloc reported nodes are not always ordered: 0, 2, 1, 3, 4
			nodes[0].next_cousin = &nodes[2];
			nodes[1].next_cousin = &nodes[3];
			nodes[2].next_cousin = &nodes[1];
			nodes[3].next_cousin = &nodes[4];
			nodes[4].next_cousin = nullptr;

			hwloc_bitmap_set_range(nodes[0].cpuset, 0, 0);
			hwloc_bitmap_set_range(nodes[1].cpuset, 1, 3);
			// hwloc_bitmap_set_range(nodes[2].cpuset, 8, 9);
			// hwloc_bitmap_set_range(nodes[3].cpuset, ...);
			// hwloc_bitmap_set_range(nodes[4].cpuset, 10, 11);
		}
		~LocalData() {
			for (auto &node : nodes) {
				hwloc_bitmap_free(node.cpuset);
			}
		}

	};
	static LocalData l;

	ASSERT_EQ(1u, depth);
	ASSERT_TRUE(idx < l.nodes.size());
	return &l.nodes[idx];
}
void hwloc_topology_destroy (hwloc_topology_t /*topology*/) {
}
