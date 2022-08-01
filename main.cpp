#include <iostream>
#include <filesystem>
#include <utility>
#include <vector>
#include <set>
#include <fstream>
#include <map>
#include <numeric>
#include "tqdm.hpp"
#include "json.hpp"

using namespace std;
namespace fs = std::filesystem;

size_t intersectionSize(const std::vector<int> &A_, const std::vector<int> &B_) {
    return std::count_if(B_.cbegin(), B_.cend(), [&](int element) {
        return std::find(A_.cbegin(), A_.cend(), element) != A_.cend();
    });
}

template<typename K, typename V>
V sumOfValues(const map<K, V> &input) {
    V acc = 0;
    for (auto[k, v]: input) {
        acc += v;
    }
    return acc;
}

enum class ChangeType {
    STAY = 1, LEAVE = 2, COPY = 3, TRANSFER = 4, NONE = -1
};

class Change {
public:
    int nodeId;
    int sourceCommunity;
    int targetCommunity;

    Change() {
        this->nodeId = -1;
        this->sourceCommunity - 1;
        this->targetCommunity - 1;
    }

    Change(int nodeId, int sourceCommunity, int targetCommunity) {
        this->nodeId = nodeId;
        this->sourceCommunity = sourceCommunity;
        this->targetCommunity = targetCommunity;
    }

    ChangeType getType() {
        if (this->sourceCommunity == -1 and this->targetCommunity == -1) return ChangeType::STAY;
        if (this->sourceCommunity == -1 and this->targetCommunity != -1) return ChangeType::COPY;
        if (this->sourceCommunity != -1 and this->targetCommunity == -1) return ChangeType::LEAVE;
        if (this->sourceCommunity != -1 and this->targetCommunity != -1) return ChangeType::TRANSFER;
        return ChangeType::NONE;
    }
};

class ChangeCounter {
public:
    int numberStays;
    int numberLeaves;
    int numberCopies;
    int numberTransfers;

    friend ostream &operator<<(ostream &os, const ChangeCounter &cc);


    void add(Change change) {
        switch (change.getType()) {
            case ChangeType::STAY:
                this->numberStays++;
                break;
            case ChangeType::LEAVE:
                this->numberLeaves++;
                break;
            case ChangeType::COPY:
                this->numberCopies++;
                break;
            case ChangeType::TRANSFER:
                this->numberTransfers++;
                break;
            case ChangeType::NONE:
                break;
        }
    }
};

ostream &operator<<(ostream &os, const ChangeCounter &cc) {
    os << "numberStays " << cc.numberStays << endl
       << "numberLeaves " << cc.numberLeaves << endl
       << "numberCopies " << cc.numberCopies << endl
       << "numberTransfers " << cc.numberTransfers;
    return os;
}

class ClusteringLoader {
private:
    fs::path filelocation;

public:
    vector<vector<int>> clusterToNode;
    vector<vector<int>> nodeToCluster;
    int maximumCommunityId;
    int num_nodes;

    ClusteringLoader(fs::path input, int num_nodes) {
        this->filelocation = std::move(input);
        this->num_nodes = num_nodes;
        this->init();
    }

    void init() {
        std::ifstream infile(this->filelocation);
        if (!infile.good()) throw invalid_argument("clustering file does not exist");
        string line;
        stringstream linestream;
        clusterToNode = vector<vector<int>>();
        nodeToCluster = vector<vector<int>>();
        // prepopulate with empty lists
        for (int i = 0; i < this->num_nodes; i++) {
            nodeToCluster.emplace_back();
        }
        int communityId;
        vector<int> node_ids = vector<int>();
        int _maxCommunityId = -1;

        while (getline(infile, line)) {
            linestream.clear();
            node_ids.clear();
            linestream << line;
            // lines read as cluster_id node_id node_id
            linestream >> communityId;
            _maxCommunityId = max(communityId, _maxCommunityId);
            for (int nodeId; linestream >> nodeId;) {
                node_ids.push_back(nodeId);
                this->nodeToCluster[nodeId].push_back(communityId);
            }
            this->clusterToNode.push_back(node_ids);
        }
        this->maximumCommunityId = _maxCommunityId;
        /*
        // check that every node has exactly one community
        for (int i = 0; i < this->num_nodes; i++) {
            if (this->nodeToCluster[i].size() != 1) {
                cout << "illegal state! node " << i << " has " << this->nodeToCluster[i].size() << " clusters" << endl;
                // throw invalid_argument("node " + i + " has "+ this->nodeToCluster[i].size() + "clusters instead of the one allowed" );
            }
        }
         */
    }
};

class GraphLoader {
private:
    fs::path filelocation;
public:
    vector<vector<int>> adj;
    int nodeCount;

    GraphLoader(fs::path input) {
        this->filelocation = std::move(input);
        this->nodeCount = -1;
        this->init();
    }

    void init() {
        vector<set<int>> adj;
        vector<vector<int>> adj2;
        std::ifstream infile(this->filelocation);
        if (!infile.good()) throw invalid_argument("input file does not exist");

        cout << "finding max node id" << endl;
        int max_id = -1;
        string line;
        stringstream line2;
        int a, b;
        while (getline(infile, line)) {
            if (line.starts_with("#")) continue;
            line2.clear();
            line2 << line;
            line2 >> a >> b;
            if (a > max_id) max_id = a;
            if (b > max_id) max_id = b;
        }

        if (max_id == -1) throw invalid_argument("could not find any nodes in the input file");
        cout << "building the adj list" << endl;
        // reopen the file
        infile = ifstream(this->filelocation);
        adj.resize(max_id + 1);
        while (getline(infile, line)) {
            if (line.starts_with("#")) continue;
            line2.clear();
            line2 << line;
            line2 >> a >> b;
            if (a == b) continue;
            adj[a].insert(b);
            adj[b].insert(a);
        }

        cout << "building vectors from sets" << endl;
        // convert to vectors
        adj2.resize(max_id + 1);
        for (int i = 0; i <= max_id; i++) {
            adj2[i] = vector(adj[i].begin(), adj[i].end());
            sort(adj2[i].begin(), adj2[i].end());
        }

        this->adj = adj2;
        this->nodeCount = max_id + 1;
    }
};

class Fox {
private:
    fs::path inputFilePath;
    fs::path outputDir;
    fs::path iterationDumpDir;
    // internally sorted
    vector<vector<int>> adjacencyList;
    vector<int> nodeIds;

    int numberNodes;
    float wccDiff;

    vector<vector<int>> hashmapNC;
    map<int, float> hashmapCWCC;
    map<int, map<int, float>> hashmapCLookup;
    vector<int> numberNeighbors;
    float cc;
    int iterationCount = 0;
    float globalWcc;
    vector<float> ccPerNode;

    int maximumCommunityId;


public:
    float threshold = 0.8;
    int queueSize = 1;
    int threadCount = 1;
    bool dumpResults = true;

    float calculateGlobalCC() {
        return accumulate(this->ccPerNode.begin(), this->ccPerNode.end(), 0.0f) / (float) this->numberNodes;
    }

    string getResultPath() {
        return this->iterationDumpDir / (to_string(this->iterationCount) + "clusters.txt");
    }

    void initializeCluster() {
        /**
         * Apply a primitive initial clustering
        * Each node, that has not been assigned a cluster yet, forms a new cluster with its adjacent nodes
        * that are not in a cluster yet
        * This is done in the order of node_ids
         */
        int maxCommunityId = -1;
        for (auto nodeId: this->nodeIds) {
            if (!this->hashmapNC[nodeId].empty()) {
                continue;
            }
            maxCommunityId++;
            this->hashmapNC[nodeId].push_back(maxCommunityId);
            for (auto neighborId: this->adjacencyList[nodeId]) {
                if (!this->hashmapNC[neighborId].empty()) continue;
                this->hashmapNC[neighborId].push_back(maxCommunityId);
            }
        }
        this->maximumCommunityId = maxCommunityId;
    }

    void removeSingleNodeCommunity(int communityId) {
        if (this->hashmapCLookup[communityId].size() > 1)
            throw invalid_argument("Community contains more than 1 node!");
        // cout << communityId << " gets erased" << endl;
        auto lastNodeId = this->hashmapCLookup[communityId].begin()->first;
        this->hashmapCLookup.erase(this->hashmapCLookup.find(communityId));
        this->hashmapCWCC.erase(this->hashmapCWCC.find(communityId));
        this->hashmapNC[lastNodeId].erase(
                remove(this->hashmapNC[lastNodeId].begin(), this->hashmapNC[lastNodeId].end(), communityId),
                this->hashmapNC[lastNodeId].end());
    }

    void removeAllSingleNodeCommunities() {
        vector<int> badCommunities;
        for (int communityId = 0; communityId < this->maximumCommunityId + 1; ++communityId) {
            if (this->hashmapCLookup.contains(communityId) && this->hashmapCLookup[communityId].size() <= 1) {
                badCommunities.push_back(communityId);
            }
        }
        for (auto communityId: badCommunities) {
            this->removeSingleNodeCommunity(communityId);
        }
    }

    Fox(const string &inputFilePath, const string &outputDir, float threshold = 0.01, int queueSize = 1,
        int threadCount = 1,
        bool dumpResults = true, bool clustering = true, string clustering_path = "") {
        this->dumpResults = dumpResults;
        this->inputFilePath = fs::path(inputFilePath);
        this->outputDir = fs::path(outputDir);
        auto currentTime = std::time(nullptr);
        stringstream timestamp;
        timestamp << put_time(localtime(&currentTime), "%c");
        string folderName = "CPP_" + this->inputFilePath.filename().string() + "_" + timestamp.str();
        this->iterationDumpDir = this->outputDir / folderName / "iterations";
        fs::create_directories(this->iterationDumpDir);

        cout << "loading input file" << endl;
        auto g = GraphLoader(this->inputFilePath);
        this->adjacencyList = g.adj;
        this->numberNodes = g.nodeCount;
        cout << "graph loaded" << endl;
        this->threshold = threshold;
        this->wccDiff = 0;

        // lazy fox specific settings
        this->queueSize = queueSize;
        this->threadCount = threadCount;
        cout << "running with " << threadCount << " threads" << endl;

        this->hashmapNC.resize(this->numberNodes);

        // preprocess
        cout << "beginning initialisation" << endl;
        cout << "counting neighbors" << endl;
        countNeighborsPerNode();
        cout << "calculating CC Per node" << endl;
        calculateCCPerNode();
        cout << "ordering notes" << endl;
        orderNodes();

        cout << "computing CC Global" << endl;
        this->cc = calculateGlobalCC();
        cout << "CC Global: " << this->cc << endl;

        if (clustering) {
            cout << "initial clustering" << endl;
            initializeCluster();
        } else {
            cout << "loading external clustering" << endl;
            ClusteringLoader loader = ClusteringLoader(fs::path(clustering_path), this->numberNodes);
            this->hashmapNC = loader.nodeToCluster;
            this->maximumCommunityId = loader.maximumCommunityId;
        }
        cout << "initial clustering produced " << this->maximumCommunityId + 1 << " communities" << endl;
        cout << "initializing clustering maps" << endl;

        initializeClusterMaps();

        cout << "global WCC" << endl;
        this->globalWcc = sumOfValues(hashmapCWCC);
        cout << "initialization done" << endl;
        cout << "cleaning broken communities" << endl;
        this->removeAllSingleNodeCommunities();
    }

    float calculateDeltaL(int nodeId, int communityId) {
        auto alteredCommunity = this->getAlteredCommunityLeave(nodeId, communityId);

        auto wccDachAlteredCommunity = this->calculateWccDachCommunity(alteredCommunity);
        auto deltaL = wccDachAlteredCommunity - hashmapCWCC[communityId];
        return deltaL;
    }

    int communityToLeave(int nodeId) {
        float bestMoveDeltaL = 0;
        int bestCId = -1;
        for (auto communityId: this->hashmapNC[nodeId]) {
            float deltaL = this->calculateDeltaL(nodeId, communityId);
            if (deltaL > bestMoveDeltaL) {
                bestMoveDeltaL = deltaL;
                bestCId = communityId;
            }
        }
        return bestCId;
    }


    float calculateDeltaJ(int nodeId, int communityId) {
        // copy constructor
        map<int, float> alteredCommunity = getAlteredCommunityJoin(nodeId, communityId);

        auto wccDachAlteredCommunity = this->calculateWccDachCommunity(alteredCommunity);
        auto deltaJ = wccDachAlteredCommunity - hashmapCWCC[communityId];
        return deltaJ;
    }

    map<int, float> getAlteredCommunityLeave(int nodeId, int communityId) {
        if (!this->hashmapCLookup.contains(communityId)) {
            throw std::invalid_argument("Community id should not exist");
        }
        // copy constructor
        map<int, float> alteredCommunity(this->hashmapCLookup[communityId]);
        for (auto neighbor: this->adjacencyList[nodeId])
            if (find(this->hashmapNC[neighbor].begin(), this->hashmapNC[neighbor].end(), communityId) !=
                this->hashmapNC[neighbor].end())
                alteredCommunity[neighbor] -= 1;
        // leave the community
        alteredCommunity.erase(nodeId);
        return alteredCommunity;
    }

    map<int, float> getAlteredCommunityJoin(int nodeId, int communityId) {
        map<int, float> alteredCommunity(hashmapCLookup[communityId]);
        // join the community
        alteredCommunity.insert(pair(nodeId, 0));

        for (auto neighbor: adjacencyList[nodeId])
            if (find(hashmapNC[neighbor].begin(), hashmapNC[neighbor].end(), communityId) !=
                hashmapNC[neighbor].end()) {
                alteredCommunity[neighbor] += 1;
                alteredCommunity[nodeId] += 1;
            }
        return alteredCommunity;
    }

    int communityToJoin(int nodeId) {
        float bestMoveDeltaJ = 0;
        int bestCId = -1;
        // we can join any community that any neighbor of us is part of, and we are not
        set<int> relevantCommunities;
        for (auto neighbor: this->adjacencyList[nodeId])
            for (auto communityId: this->hashmapNC[neighbor])
                relevantCommunities.insert(communityId);
        // remove all communities that are part of
        for (auto communityId: this->hashmapNC[nodeId])
            relevantCommunities.erase(communityId);

        for (auto communityId: relevantCommunities) {
            float deltaJ = this->calculateDeltaJ(nodeId, communityId);
            if (deltaJ > bestMoveDeltaJ) {
                bestMoveDeltaJ = deltaJ;
                bestCId = communityId;
            }
        }
        return bestCId;
    }

    Change decide(int nodeId) {

        auto change = Change(nodeId, -1, -1);
        change.sourceCommunity = this->communityToLeave(nodeId);
        change.targetCommunity = this->communityToJoin(nodeId);
        return change;
    }


    void apply(Change change) {
        // apply join if present
        if (change.getType() == ChangeType::TRANSFER || change.getType() == ChangeType::COPY) {
            auto alteredCommunity = this->getAlteredCommunityJoin(change.nodeId, change.targetCommunity);

            float newWcc = calculateWccDachCommunity(alteredCommunity);
            this->wccDiff += newWcc - this->hashmapCWCC[change.targetCommunity];

            this->hashmapCLookup[change.targetCommunity] = alteredCommunity;
            this->hashmapCWCC[change.targetCommunity] = newWcc;

            this->hashmapNC[change.nodeId].push_back(change.targetCommunity);
        }
        // apply leave if present
        if (change.getType() == ChangeType::TRANSFER || change.getType() == ChangeType::LEAVE) {
            auto alteredCommunity = this->getAlteredCommunityLeave(change.nodeId, change.sourceCommunity);

            float newWcc = calculateWccDachCommunity(alteredCommunity);
            this->wccDiff += newWcc - this->hashmapCWCC[change.sourceCommunity];

            this->hashmapCLookup[change.sourceCommunity] = alteredCommunity;
            this->hashmapCWCC[change.sourceCommunity] = newWcc;

            this->hashmapNC[change.nodeId].erase(
                    remove(this->hashmapNC[change.nodeId].begin(), this->hashmapNC[change.nodeId].end(),
                           change.sourceCommunity), this->hashmapNC[change.nodeId].end());
        }
    }


    void dumpResultsToFile(ChangeCounter counter, double duration) {
        ofstream output;

        output.open(this->iterationDumpDir / (to_string(this->iterationCount) + "clusters.txt"));
        for (int clusterId = 0; clusterId < this->maximumCommunityId; clusterId++) {
            if (!this->hashmapCLookup.contains(clusterId)) continue;

            for (auto[k, v]: hashmapCLookup[clusterId]) {
                output << k << "\t";
            }

            output << endl;
        }
        output.close();

        output.open(this->iterationDumpDir / (to_string(this->iterationCount) + ".json"));


        nlohmann::json changeCounter = {
                {"copy",     counter.numberCopies},
                {"transfer", counter.numberTransfers},
                {"leave",    counter.numberLeaves},
                {"stay",     counter.numberStays}};
        nlohmann::json cwcc = nlohmann::json::object();
        for (const auto &a: this->hashmapCWCC) {
            cwcc[to_string(a.first)] = a.second;
        }

        nlohmann::json out = {{"change_counter", changeCounter},
                              {"iteration",      this->iterationCount},
                              {"runtime",        duration},
                              {"wcc_lookup",     cwcc},
        };
        output << out.dump(4) << endl;
        output.close();
    }

    void run() {
        cout << "starting the main loop" << endl;
        cout << this->threshold << " is the threshold" << endl;
        while (true) {
            auto changeCounter = ChangeCounter();
            this->wccDiff = 0.0f;
            auto start = std::chrono::system_clock::now();

            auto chunkIdx = tq::trange(this->numberNodes / this->queueSize);
            chunkIdx.set_prefix("Processing Chunk ");
            chunkIdx.set_ostream(cout);
            vector<Change> changes;
            for (auto chunkId: chunkIdx) {
                changes.clear();
                changes.resize(this->queueSize);
                int offset;

#pragma omp parallel for default(none) num_threads(this->threadCount) private(offset) shared(chunkId, changes, changeCounter)
                for (offset = 0; offset < this->queueSize; offset++) {
                    int nodePosition = chunkId * this->queueSize + offset;
                    if (nodePosition >= this->numberNodes) continue;
                    int nodeId = this->nodeIds[nodePosition];
                    auto change = this->decide(nodeId);
                    changeCounter.add(change);
                    changes[offset] = change;
                }

                // apply changes
                for (auto change: changes) {
                    this->apply(change);
                }
            }

            this->removeAllSingleNodeCommunities();

            cout << "iteration " << iterationCount << " done" << endl;
            cout << changeCounter << endl;

            float relativeChange = ((float) this->wccDiff) / (float) this->globalWcc;
            this->globalWcc += this->wccDiff;
            cout << "relative change " << relativeChange << endl;

            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = end - start;
            cout << "epoch took " << elapsed_seconds.count() << "s" << endl;

            if (this->dumpResults) {
                cout << "dumping results..." << endl;
                this->dumpResultsToFile(changeCounter, elapsed_seconds.count());
                cout << "results dumped successfully" << endl;
            }

            if (relativeChange < threshold) break;
            this->iterationCount++;
        }
    }

    float calculateWccDach(int nodeId, const map<int, float> &community) {
        /**
         *  Compute the likelihood of a node to be part of a community as described in 3.2 (6)
        * :param node_id: the nodeId of the node, must be part of the community
        * :param community: the community represented as the hashmap from c_wcc
        * :return: the likelihood score WCC Dach
         */
        if (community.size() <= 1) return 0;
        auto nodeDegree = this->numberNeighbors[nodeId];
        // nodes with one edge or less can not form triangles
        // assuming that we do not put those nodes in a community
        if (nodeDegree <= 1) return 0;


        auto nodeDegreeToCommunity = community.at(nodeId);
        auto node_degree_to_graph_without_community = (float) nodeDegree - nodeDegreeToCommunity;
        float edges_in_community = sumOfValues(community) / 2.0f;

        auto possible_edges_in_community = (float) (community.size() * (community.size() - 1) / 2.0f);
        float community_density = edges_in_community / possible_edges_in_community;

        float expected_triangles_with_community =
                (nodeDegreeToCommunity * (nodeDegreeToCommunity - 1) / 2.0f) * community_density;

        float expected_triangles_with_graph = (nodeDegree * (nodeDegree - 1) / 2.0f) * this->cc;

        float wcc_dach = (expected_triangles_with_community / expected_triangles_with_graph) * nodeDegree / (
                community.size() - 1 + node_degree_to_graph_without_community);
        return wcc_dach;
    }

    float calculateWccDachCommunity(const map<int, float> &community) {
        /**
         * Computes the likelihood of a community to exist by summing the WCC Dach scores from all its nodes
         */
        float acc = 0;
        for (auto[node_id, nodes_in_community]: community) {
            acc += calculateWccDach(node_id, community);
        }
        return acc;
    }

    void initializeClusterMaps() {
        /**Initialized the following lookups:
        * 1. From community to node and from that node to its edge count within the community
        * 2. From community to its WCC score
        */
        for (auto nodeId: this->nodeIds) {
            // nodes have exactly one community at this stage
            auto community = this->hashmapNC[nodeId].front();
            auto cmi = this->hashmapCLookup[community];
            cmi.insert(pair(nodeId, 0));
            this->hashmapCLookup[community] = cmi;
        }

        for (int communityId = 0; communityId < this->maximumCommunityId + 1; communityId++) {
            auto cmi = this->hashmapCLookup[communityId];
            vector<int> nodesInCommunity;
            nodesInCommunity.reserve(cmi.size());
            for (auto elem: cmi) nodesInCommunity.push_back(elem.first);
            for (auto[node_id, wcc_in_community]: cmi) {
                cmi[node_id] = (float) intersectionSize(nodesInCommunity, this->adjacencyList[node_id]);
            }
            this->hashmapCLookup[communityId] = cmi;
        }

        for (int communityId = 0; communityId < this->maximumCommunityId + 1; communityId++) {
            auto community = this->hashmapCLookup[communityId];
            this->hashmapCWCC[communityId] = this->calculateWccDachCommunity(community);
        }
        cout << "finished the hashmap population" << endl;
    }


    void orderNodes() {
        this->nodeIds = vector<int>(this->numberNodes);
        std::iota(this->nodeIds.begin(), this->nodeIds.end(), 0);
        std::stable_sort(this->nodeIds.begin(), this->nodeIds.end(), [this](int a, int b) {
            return this->numberNeighbors[a] > this->numberNeighbors[b];
        });

        std::stable_sort(this->nodeIds.begin(), this->nodeIds.end(), [this](int a, int b) {
            return this->ccPerNode[a] > this->ccPerNode[b];
        });
    }


    void countNeighborsPerNode() {// count neighbors per node once
        numberNeighbors.resize(numberNodes);
        for (int i = 0; i < numberNodes; ++i) {
            numberNeighbors[i] = (int) adjacencyList[i].size();
        }
    }

    int countIntersection(vector<int> v1, vector<int> v2) {
        auto p1 = 0;
        auto p2 = 0;
        int counter = 0;
        while (p1 != v1.size() && p2 != v2.size()) {
            if (v1.at(p1) == v2.at(p2)) counter++;
            if (v1.at(p1) < v2.at(p2)) p1++; else p2++;
        }
        return counter;
    }

    void calculateCCPerNode() {
        // calculate cc per node once
        this->ccPerNode.resize(numberNodes);
        int nodeId;
        int tmp = 0;
#pragma omp parallel for default(none) num_threads(this->threadCount) private(nodeId) shared(cout, tmp)
        for (nodeId = numberNodes - 1; nodeId >= 0; nodeId--) {
            tmp++;
            if (tmp % (numberNodes / 100) == 0) {
                cout << tmp / (numberNodes / 100) << " % have a cc \t\r" << flush;
            }

            if (numberNeighbors[nodeId] <= 1) {
                this->ccPerNode[nodeId] = 0;
                continue;
            }
            //k denotes the number of edges between two neighbors of node i
            int k = 0;
            for (auto neighborId: adjacencyList[nodeId]) {
                // this works because our adjacencyLists are sorted
                k += countIntersection(adjacencyList[nodeId], adjacencyList[neighborId]);
            }
            // Pigeonhole principle
            // therefore we drop a factor of 2 here
            this->ccPerNode[nodeId] = ((float) k /
                                       ((float) numberNeighbors[nodeId] * (float) (numberNeighbors[nodeId] - 1)));
        }
        cout << "done" << endl;
    }
};

int main(int argc, char *argv[]) {
    cout << "Starting LazyFox" << std::endl;
    auto start = std::chrono::system_clock::now();

    if (argc < 3) {
        cout
                << "usage: ./LazyFOX input_graph output_directory [queue_size] [thread_count] [dumping={0,1}] [clustering={0,1} path_to_clustering] [post-processing={0,1}  path_to_post_processor]\n"
                   "\n"
                   "Run the LazyFOX algorithm\n"
                   "\n"
                   "arguments:\n"
                   "  input_graph         File containing the dataset as an edge list\n"
                   "  output_directory    Directory to log into, a subdirectory will be created for each run\n"
                   "\n"
                   "optional arguments:\n"
                   "  queue_size          Specify the degree of parallel processing\n"
                   "  thread_count        Specify how many threads to use. Should be below or equal to queue_size\n"
                   "  dumping             If set to 0, LazyFOX will not save clustering results to disk\n"
                   "  clustering          If set to 0, LazyFOX will use an external clustering.\n"
                   "  post-processing     If set to 0, LazyFOX will use an external script for postprocessing " << endl;
        return -1;
    }

    string input = argv[1];
    string output = argv[2];

    int threadCount = 1;
    int qSize = 1;
    if (argc > 3) qSize = stoi(argv[3]);
    if (argc > 4) threadCount = stoi(argv[4]);
    bool dumping = true;
    if (argc > 5) dumping = stoi(argv[5]);
    int arg_counter = 6;

    bool clustering = true;
    string clustering_path;
    if (argc > arg_counter) {
        clustering = stoi(argv[arg_counter]);
        arg_counter++;
        if (!clustering) {
            clustering_path = argv[arg_counter];
            arg_counter++;
        }
    }

    bool post_processing = true;
    string postprocessing_path;
    if (argc > arg_counter) {
        post_processing = stoi(argv[arg_counter]);
        arg_counter++;
        if (!post_processing) {
            postprocessing_path = argv[arg_counter];
            arg_counter++;
        }
    }

    cout << "running lazyfox with input " << input << " and output " << output << "processing a queue size of " << qSize
         << " and working with " << threadCount << " threads" << endl;
    cout << "dumping is " << (dumping ? "enabled" : "disabled") << endl;
    cout << "clustering is " << (clustering ? "enabled" : "loaded from ") << clustering_path << endl;
    cout << "postprocessing is " << (post_processing ? "skipped" : "external using ") << postprocessing_path << endl;
    auto fox = Fox(input, output, 0.01, qSize, threadCount, dumping, clustering, clustering_path);

    fox.run();

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);

    std::cout << "finished computation at " << std::ctime(&end_time)
              << "elapsed time: " << elapsed_seconds.count() << "s" << endl;
    if (!post_processing) {
        std::cout << "Starting external processing";
        string final_result_path = fox.getResultPath();
        string command = postprocessing_path + " \"" + final_result_path + "\"";
        std::cout << "Calling " << command;
        system(command.c_str());
    }
    return 0;
}