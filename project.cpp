#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <limits>
#include <algorithm>
#include <iomanip>

using namespace std;

// ---------------- MESSAGE ----------------
struct Message {
    int id;
    string data;
    string routeTrace;

    Message(int id, string data, string trace = "")
        : id(id), data(data), routeTrace(trace) {}
};

// ---------------- TOPIC ----------------
class Topic {
private:
    string name;
    vector<Message> messages;
    int nextMessageId = 1;

public:
    Topic(string name) : name(name) {}

    void publish(string data, string routeTrace) {
        messages.emplace_back(nextMessageId++, data, routeTrace);
    }

    bool consume(int& offset, Message& result) {
        if (offset >= (int)messages.size()) return false;
        result = messages[offset++];
        return true;
    }

    int getPendingCount(int offset) const {
        return max(0, (int)messages.size() - offset);
    }
};

// ---------------- BROKER NODE ----------------
class BrokerNode {
private:
    string nodeId;
    unordered_map<string, Topic*> topics;

public:
    BrokerNode(string id) : nodeId(id) {}

    ~BrokerNode() {
        for (auto& pair : topics) delete pair.second;
    }

    string getId() const { return nodeId; }

    void createTopic(string topicName) {
        if (topics.find(topicName) == topics.end()) {
            topics[topicName] = new Topic(topicName);
        }
    }

    Topic* getTopic(string topicName) {
        if (topics.find(topicName) == topics.end()) return nullptr;
        return topics[topicName];
    }
};

// ---------------- DISTRIBUTED CLUSTER (GRAPH + DIJKSTRA) ----------------
struct Edge {
    string targetBroker;
    int latencyMs;
};

class BrokerCluster {
private:
    unordered_map<string, BrokerNode*> nodes;
    unordered_map<string, vector<Edge>> adjList;
    vector<string> activeTopics;

public:
    ~BrokerCluster() {
        for (auto& pair : nodes) delete pair.second;
    }

    bool brokerExists(const string& id) const {
        return nodes.find(id) != nodes.end();
    }

    bool topicExists(const string& name) const {
        return find(activeTopics.begin(), activeTopics.end(), name) != activeTopics.end();
    }

    void addBroker(string brokerId) {
        if (brokerExists(brokerId)) {
            cout << "  [!] Broker '" << brokerId << "' already exists.\n";
            return;
        }
        nodes[brokerId] = new BrokerNode(brokerId);
        for (const string& t : activeTopics) {
            nodes[brokerId]->createTopic(t);
        }
        cout << "  [+] Added Broker Node: " << brokerId << "\n";
    }

    void connectBrokers(string u, string v, int latencyMs) {
        if (!brokerExists(u) || !brokerExists(v)) {
            cout << "  [!] One or both broker IDs do not exist.\n";
            return;
        }
        if (u == v) {
            cout << "  [!] Self-loops are not allowed.\n";
            return;
        }
        // Check if link already exists; if so, update it
        for (auto& edge : adjList[u]) {
            if (edge.targetBroker == v) {
                updateLatency(u, v, latencyMs);
                return;
            }
        }
        adjList[u].push_back({v, latencyMs});
        adjList[v].push_back({u, latencyMs});
        cout << "  [+] Connected " << u << " <---> " << v << " (" << latencyMs << " ms)\n";
    }

    bool updateLatency(string u, string v, int newLatency) {
        bool found = false;
        for (auto& edge : adjList[u]) {
            if (edge.targetBroker == v) {
                edge.latencyMs = newLatency;
                found = true;
            }
        }
        for (auto& edge : adjList[v]) {
            if (edge.targetBroker == u) {
                edge.latencyMs = newLatency;
            }
        }
        if (found) {
            cout << "  [~] Link " << u << " <---> " << v << " latency set to " << newLatency << " ms\n";
        } else {
            cout << "  [!] No direct link exists between " << u << " and " << v << ".\n";
        }
        return found;
    }

    void createGlobalTopic(string topicName) {
        if (topicExists(topicName)) {
            cout << "  [!] Topic '" << topicName << "' already exists.\n";
            return;
        }
        activeTopics.push_back(topicName);
        for (auto& pair : nodes) {
            pair.second->createTopic(topicName);
        }
        cout << "  [+] Topic '" << topicName << "' created across all brokers.\n";
    }

    BrokerNode* getBroker(string brokerId) {
        if (!brokerExists(brokerId)) return nullptr;
        return nodes[brokerId];
    }

    // Dijkstra's Shortest Path Algorithm
    pair<int, vector<string>> findOptimalRoute(string src, string dest) {
        unordered_map<string, int> dist;
        unordered_map<string, string> parent;

        for (auto& pair : nodes) {
            dist[pair.first] = numeric_limits<int>::max();
        }

        priority_queue<pair<int, string>, vector<pair<int, string>>, greater<>> pq;
        dist[src] = 0;
        pq.push({0, src});

        while (!pq.empty()) {
            int d = pq.top().first;
            string u = pq.top().second;
            pq.pop();

            if (d > dist[u]) continue;
            if (u == dest) break;

            for (const auto& edge : adjList[u]) {
                string v = edge.targetBroker;
                int weight = edge.latencyMs;

                if (dist[u] != numeric_limits<int>::max() && dist[u] + weight < dist[v]) {
                    dist[v] = dist[u] + weight;
                    parent[v] = u;
                    pq.push({dist[v], v});
                }
            }
        }

        vector<string> path;
        if (dist[dest] == numeric_limits<int>::max()) return {-1, path};

        for (string curr = dest; curr != ""; curr = parent[curr]) {
            path.push_back(curr);
            if (curr == src) break;
        }
        reverse(path.begin(), path.end());
        return {dist[dest], path};
    }

    void routeMessage(string srcBroker, string destBroker, string topicName, string data) {
        if (!brokerExists(srcBroker) || !brokerExists(destBroker)) {
            cout << "  [!] Invalid source or destination broker.\n";
            return;
        }
        if (!topicExists(topicName)) {
            cout << "  [!] Topic '" << topicName << "' does not exist.\n";
            return;
        }

        auto result = findOptimalRoute(srcBroker, destBroker);
        int totalLatency = result.first;
        vector<string> path = result.second;

        if (totalLatency == -1) {
            cout << "  [X] MESSAGE DROPPED: No active route between " << srcBroker << " and " << destBroker << ".\n";
            return;
        }

        string trace = "";
        for (size_t i = 0; i < path.size(); ++i) {
            trace += path[i] + (i + 1 < path.size() ? " -> " : "");
        }
        trace += " [" + to_string(totalLatency) + " ms]";

        Topic* targetTopic = nodes[destBroker]->getTopic(topicName);
        if (targetTopic) {
            targetTopic->publish(data, trace);
            cout << "  [OK] Message Routed Successfully!\n";
            cout << "       Optimal Path : " << trace << "\n";
        }
    }

    void displayTopology() const {
        cout << "\n  --- LIVE CLUSTER NETWORK TOPOLOGY (GRAPH ADJACENCY LIST) ---\n";
        if (nodes.empty()) {
            cout << "  (No brokers active)\n";
            return;
        }
        for (const auto& pair : nodes) {
            string u = pair.first;
            cout << "  [" << left << setw(12) << u << "] ---> ";
            auto it = adjList.find(u);
            if (it == adjList.end() || it->second.empty()) {
                cout << "(Isolated Node)";
            } else {
                for (size_t i = 0; i < it->second.size(); ++i) {
                    cout << it->second[i].targetBroker << "(" << it->second[i].latencyMs << "ms)";
                    if (i + 1 < it->second.size()) cout << ", ";
                }
            }
            cout << "\n";
        }
        cout << "  Active Topics: ";
        for (size_t i = 0; i < activeTopics.size(); ++i) {
            cout << activeTopics[i] << (i + 1 < activeTopics.size() ? ", " : "");
        }
        cout << "\n";
    }
};

// ---------------- DASHBOARD HELPERS ----------------
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pauseScreen() {
    cout << "\n  Press Enter to return to dashboard...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

int getSafeInt(const string& prompt) {
    int val;
    while (true) {
        cout << prompt;
        if (cin >> val && val >= 0) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return val;
        }
        cout << "  [!] Invalid number. Try again.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string getSafeLine(const string& prompt) {
    string line;
    cout << prompt;
    getline(cin, line);
    return line;
}

void renderHeader() {
    cout << "====================================================================\n";
    cout << "     DISTRIBUTED LATENCY-AWARE MESSAGE BROKER (DIJKSTRA ROUTER)     \n";
    cout << "====================================================================\n";
}

// ---------------- MAIN DASHBOARD ----------------
int main() {
    BrokerCluster cluster;

    // Track consumer offsets per {broker + ":" + topic}
    unordered_map<string, int> consumerOffsets;

    // Pre-seed default topology so dashboard is immediately usable
    cluster.addBroker("Delhi");
    cluster.addBroker("Mumbai");
    cluster.addBroker("Hyderabad");
    cluster.addBroker("Bengaluru");

    cluster.connectBrokers("Delhi", "Mumbai", 25);
    cluster.connectBrokers("Delhi", "Hyderabad", 30);
    cluster.connectBrokers("Mumbai", "Bengaluru", 20);
    cluster.connectBrokers("Hyderabad", "Bengaluru", 10);
    cluster.connectBrokers("Delhi", "Bengaluru", 65);

    cluster.createGlobalTopic("orders");
    cluster.createGlobalTopic("payments");

    while (true) {
        clearScreen();
        renderHeader();
        cluster.displayTopology();
        cout << "--------------------------------------------------------------------\n";
        cout << "  [1] Publish Message (Producer -> Optimal Dijkstra Route)\n";
        cout << "  [2] Consume Messages (Read Queue at Destination Broker)\n";
        cout << "  [3] Simulate Congestion / Update Link Latency\n";
        cout << "  [4] Add New Broker Node (Graph Vertex)\n";
        cout << "  [5] Add New Network Link (Weighted Edge)\n";
        cout << "  [6] Create New Topic\n";
        cout << "  [7] Test Shortest Path Only (Dry-Run Route Finder)\n";
        cout << "  [0] Exit\n";
        cout << "--------------------------------------------------------------------\n";

        int choice = getSafeInt("  Select Option > ");

        if (choice == 0) {
            cout << "\n  Shutting down broker cluster. Goodbye!\n";
            break;
        }

        cout << "\n";
        switch (choice) {
            case 1: {
                string src = getSafeLine("  Source Broker (e.g., Delhi)        : ");
                string dest = getSafeLine("  Target Broker (e.g., Bengaluru)    : ");
                string topic = getSafeLine("  Topic Name (e.g., orders)          : ");
                string payload = getSafeLine("  Message Payload                    : ");
                cluster.routeMessage(src, dest, topic, payload);
                pauseScreen();
                break;
            }
            case 2: {
                string brokerId = getSafeLine("  Consumer's Broker Node             : ");
                string topicName = getSafeLine("  Topic Name to Consume              : ");

                BrokerNode* broker = cluster.getBroker(brokerId);
                if (!broker) {
                    cout << "  [!] Broker not found.\n";
                    pauseScreen();
                    break;
                }
                Topic* topic = broker->getTopic(topicName);
                if (!topic) {
                    cout << "  [!] Topic not found.\n";
                    pauseScreen();
                    break;
                }

                string key = brokerId + ":" + topicName;
                int& offset = consumerOffsets[key];
                Message msg(0, "");
                int count = 0;

                cout << "\n  --- CONSUMED MESSAGES AT [" << brokerId << " | #" << topicName << "] ---\n";
                while (topic->consume(offset, msg)) {
                    cout << "  Msg #" << msg.id << " | Data: \"" << msg.data 
                         << "\" | Route: " << msg.routeTrace << "\n";
                    count++;
                }
                if (count == 0) {
                    cout << "  (No new unread messages in queue)\n";
                }
                pauseScreen();
                break;
            }
            case 3: {
                string u = getSafeLine("  First Broker Node                  : ");
                string v = getSafeLine("  Second Broker Node                 : ");
                int latency = getSafeInt("  New Latency in ms (e.g., 999)      : ");
                cluster.updateLatency(u, v, latency);
                pauseScreen();
                break;
            }
            case 4: {
                string id = getSafeLine("  New Broker Node Name (e.g., Pune)  : ");
                cluster.addBroker(id);
                pauseScreen();
                break;
            }
            case 5: {
                string u = getSafeLine("  First Broker Node                  : ");
                string v = getSafeLine("  Second Broker Node                 : ");
                int latency = getSafeInt("  Link Latency (ms)                  : ");
                cluster.connectBrokers(u, v, latency);
                pauseScreen();
                break;
            }
            case 6: {
                string topic = getSafeLine("  New Topic Name                     : ");
                cluster.createGlobalTopic(topic);
                pauseScreen();
                break;
            }
            case 7: {
                string src = getSafeLine("  Start Broker                       : ");
                string dest = getSafeLine("  End Broker                         : ");
                if (!cluster.brokerExists(src) || !cluster.brokerExists(dest)) {
                    cout << "  [!] Invalid broker names.\n";
                } else {
                    auto res = cluster.findOptimalRoute(src, dest);
                    if (res.first == -1) {
                        cout << "  [X] No path exists between " << src << " and " << dest << ".\n";
                    } else {
                        cout << "  [*] Optimal Path: ";
                        for (size_t i = 0; i < res.second.size(); ++i) {
                            cout << res.second[i] << (i + 1 < res.second.size() ? " -> " : "");
                        }
                        cout << " (Total Latency: " << res.first << " ms)\n";
                    }
                }
                pauseScreen();
                break;
            }
            default:
                cout << "  [!] Invalid option selected.\n";
                pauseScreen();
                break;
        }
    }

    return 0;
}