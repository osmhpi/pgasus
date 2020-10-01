#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <numeric>
#include <map>

#include <omp.h>

#include "PGASUS/malloc.hpp"

#include "exec.hpp"

namespace
{

class OpenMPThreadDist
{
public:
	/** Can PGASUS work sensibly with the current OpenMP places configuration? */
	static bool isValid() {
		return instance().m_isValid;
	}
	static const std::vector<std::vector<int>>& numaNodeToOMPPlaces() {
		return instance().m_numaNodeToOMPPlaces;
	}
	static const std::vector<int>& ompPlaceToNumaNode() {
		return instance().m_ompPlaceToNumaNode;
	}
	static const std::vector<unsigned>& threadsPerOMPPlace() {
		return instance().m_threadsPerOMPPlace;
	}

	static void print() {
		const auto& map = numaNodeToOMPPlaces();
		std::cerr << "OpenMP places per NUMA nodes (and their default thread counts): " << std::endl;
		if (map.empty()) {
			std::cerr << "  -- OMP_PLACES NOT CONFIGURED! --" << std::endl;
		}
		for (size_t n = 0; n < map.size(); ++n) {
			std::cerr << "  Node " << n << ": ";
			for (auto it = map[n].begin(); it != map[n].end(); ++it) {
				std::cerr << *it << " (" << threadsPerOMPPlace()[unsigned(*it)] << ")";
				if (it+1 != map[n].end()) {
					std::cerr << ", ";
				}
			}
			std::cerr << std::endl;
		}
	}

private:
	OpenMPThreadDist()
		: m_isValid{ false }
	{
		assert(!omp_in_parallel());
		const auto env_OMP_PLACES = std::getenv("OMP_PLACES");
		if (!env_OMP_PLACES || env_OMP_PLACES[0] == '\0') {
			std::cerr << "WARNING: Set environment OMP_PLACES=sockets or similar "
				"for expected behavior of OpenMP on NUMA systems." << std::endl;
			return;
		}
		m_isValid = true;
		#pragma omp parallel
		{
			const auto numaNode = numa::Node::curr();
			const unsigned physicalNumaNodeId = unsigned(numaNode.physicalId());
			assert(numaNode.valid());

			if (omp_get_place_num() < 0) {
				#pragma omp single
				std::cerr << "OMP_PLACES not configured!" << std::endl;
				m_isValid = false;
			}
			else {
				const int ompPlace = unsigned(omp_get_place_num());
				#pragma omp critical
				{
					m_threadsPerOMPPlace.resize(std::max(m_threadsPerOMPPlace.size(), size_t(ompPlace + 1)));
					++m_threadsPerOMPPlace[ompPlace];
					m_numaNodeToOMPPlaces.resize(std::max(m_numaNodeToOMPPlaces.size(), size_t(physicalNumaNodeId + 1)));
					auto &places = m_numaNodeToOMPPlaces[physicalNumaNodeId];
					if (places.end() == std::find(places.begin(), places.end(), ompPlace)) {
						places.push_back(ompPlace);
						std::sort(places.begin(), places.end());
					}
					m_ompPlaceToNumaNode.resize(std::max(m_ompPlaceToNumaNode.size(), size_t(ompPlace + 1)), -1);
					auto &placeNode = m_ompPlaceToNumaNode[ompPlace];
					if (placeNode != -1 && placeNode != int(physicalNumaNodeId)) {
						std::cerr << "OpenMP place " << ompPlace << " spread across multiple NUMA nodes!" << std::endl
							<< "(Seen on " << placeNode << " and " << physicalNumaNodeId << ")" << std::endl;
						m_isValid = false;
					}
					else {
						placeNode = int(physicalNumaNodeId);
					}
				}
			}
		}
	}

private:
	static const OpenMPThreadDist& instance() {
		static OpenMPThreadDist inst;
		return inst;
	}

private:
	bool m_isValid;
	std::vector<std::vector<int>> m_numaNodeToOMPPlaces;
	std::vector<int> m_ompPlaceToNumaNode;
	std::vector<unsigned> m_threadsPerOMPPlace;
};

}

class PgasOmpExecutor : public Executor {
	struct NodeStorage {
		std::vector<std::unique_ptr<TextFile>> files;
		std::map<std::string, TextFile*> filesMap;
	};
	std::vector<std::unique_ptr<NodeStorage>> nodeStorages;

public:

	PgasOmpExecutor() {
		OpenMPThreadDist::print();
		if (!OpenMPThreadDist::isValid()) {
			std::cerr << "OpenMP places configuration is invalid. Aborting." << std::endl;
			exit(1);
		}
	}

	virtual std::vector<TextFile*> allFiles() {
		std::vector<TextFile*> f;
		for (auto& nodeStorage : nodeStorages) {
			for (auto &file : nodeStorage->files) {
				f.push_back(file.get());
			}
		}
		return f;
	}

	virtual void loadFiles(const std::vector<std::string> &fileNames) {
		omp_set_nested(1);
		const auto& numaNodes = numa::NodeList::logicalNodesWithCPUs();
		const size_t totalCPUCount = std::accumulate(
			numaNodes.begin(), numaNodes.end(), size_t(0),
			[] (size_t init, const numa::Node& node) {
				return init + node.cpuCount(); });

		std::vector<std::vector<std::string>> perNodeFileNames(numaNodes.size());

		// Distribute files/jobs to NUMA nodes according to local number of CPU cores
		const float distFactor = float(fileNames.size()) / totalCPUCount;
		size_t nextFileName = 0u;
		for (size_t node = 0; node < numaNodes.size(); ++node) {
			const size_t localCount =
				size_t(std::ceil(numaNodes[node].cpuCount() * distFactor));
			for (size_t l = 0; l < localCount && nextFileName < fileNames.size();
				++l, ++nextFileName) {
				perNodeFileNames[node].push_back(fileNames[nextFileName]);
			}
		}

		nodeStorages.resize(numa::NodeList::logicalNodesCount());

		#pragma omp parallel proc_bind(spread) num_threads(numaNodes.size())
		{
			const auto numaNode = numa::Node::curr();
			const numa::PlaceGuard placeGuard{ numaNode };
			const auto numaNodeId = numaNode.logicalId();
			assert(!nodeStorages[numaNodeId]);
			nodeStorages[numaNodeId] = std::unique_ptr<NodeStorage>(new NodeStorage);
			NodeStorage &nodeStorage = *nodeStorages[numaNodeId];
			const auto& localFileNames = perNodeFileNames[numaNodeId];

			#pragma omp parallel for proc_bind(master) num_threads(numaNode.threadCount())
			for (size_t	i = 0; i < localFileNames.size(); ++i) {
				const std::string& fileName = localFileNames[i];
				auto f = std::unique_ptr<TextFile>(new TextFile(fileName));
				auto fPtr = f.get();
				#pragma omp critical(fileaccess)
				{
					nodeStorage.files.push_back(std::move(f));
					nodeStorage.filesMap.emplace(fileName, fPtr);
				}
			}
		}
	}

	virtual void topWords(const std::vector<std::string> &fileNames) {
		omp_set_nested(1);
		const auto& numaNodes = numa::NodeList::logicalNodesWithCPUs();

		struct SmallerOp {
			using Type1 = std::pair<const std::string, TextFile*>;
			using Type2 = std::string;
			bool operator()(const Type1& lhs, const Type2& rhs) const {
				return lhs.first < rhs;
			}
			bool operator()(const Type2& lhs, const Type1& rhs) const {
				return lhs < rhs.first;
			}
		};

		struct BackInserter {
			BackInserter(std::vector<const TextFile*> & container)
				: container{ container } {}
			BackInserter& operator=(const std::pair<const std::string, TextFile*>& pair) {
				container.push_back(pair.second);
				return *this;
			}
			BackInserter& operator*() { return *this; }
			BackInserter& operator++() { return *this; }
			BackInserter& operator++(int) { return *this; }
			std::vector<const TextFile*> & container;
		};

		#pragma omp parallel proc_bind(spread) num_threads(numaNodes.size())
		{
			const auto numaNode = numa::Node::curr();
			const numa::PlaceGuard placeGuard{ numaNode };

			const std::map<std::string, TextFile*> &allLocalFiles
				= nodeStorages[numaNode.logicalId()]->filesMap;

			std::vector<const TextFile*> localFiles;

			std::set_intersection(allLocalFiles.begin(), allLocalFiles.end(),
				fileNames.begin(), fileNames.end(),
				BackInserter(localFiles),
				SmallerOp());

			#pragma omp parallel for proc_bind(master) num_threads(numaNode.threadCount())
			for (size_t i = 0; i < localFiles.size(); ++i) {
				assert(numaNode == numa::Node::curr());
				localFiles[i]->countWords();
			}
		}
	}

	virtual std::vector<size_t> countWords(const std::vector<std::string> &words) {
		omp_set_nested(1);
		const auto& numaNodes = numa::NodeList::logicalNodesWithCPUs();

		std::vector<size_t> globalCounts(words.size(), 0);
		std::vector<omp_lock_t> localLocks(numaNodes.size());
		const size_t numWords = words.size();

		#pragma omp parallel proc_bind(spread) num_threads(numaNodes.size())
		{
			const auto numaNode = numa::Node::curr();
			const numa::PlaceGuard placeGuard{ numaNode };
			const std::vector<std::unique_ptr<TextFile>>& localFiles =
				nodeStorages[numaNode.logicalId()]->files;

			std::vector<size_t> localCounts(numWords);

			omp_lock_t* localLock = &localLocks[numaNode.logicalId()];
			omp_init_lock(localLock);

			#pragma omp parallel for proc_bind(master) num_threads(numaNode.threadCount())
			for (size_t i = 0; i < localFiles.size(); ++i) {
				assert(numaNode == numa::Node::curr());
				std::vector<size_t> threadLocalCounts(numWords);
				for (size_t wi = 0; wi < numWords; ++wi) {
					threadLocalCounts[wi] = localFiles[i]->count(words[wi]);
				}
				omp_set_lock(localLock);
				for (size_t wi = 0; wi < numWords; ++wi) {
					localCounts[wi] += threadLocalCounts[wi];
				}
				omp_unset_lock(localLock);
			}

			omp_destroy_lock(localLock);

			#pragma omp critical
			for (size_t i = 0; i < words.size(); ++i) {
				globalCounts[i] += localCounts[i];
			}
		}
		return globalCounts;
	}
};

std::unique_ptr<Executor> createExecutor() {
	return std::unique_ptr<Executor>(new PgasOmpExecutor());
}
