#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <unistd.h>
#include <vector>

using namespace std;

// Menu Item Class
class MenuItem {
public:
  int id;
  string name;
  int prepTime; // Preparation time in minutes
  int eatTime;  // Eating time in minutes

  MenuItem(int id, string name, int prepTime, int eatTime)
      : id(id), name(name), prepTime(prepTime), eatTime(eatTime) {}
};

// Menu Class
class Menu {
private:
  vector<MenuItem> menuItems;

public:
  void loadMenuFromFile(const string &filename) {
    ifstream file(filename);
    if (!file) {
      cerr << "Error opening menu file!" << endl;
      exit(1);
    }

    int id, prepTime, eatTime;
    string name;
    while (file >> id >> name >> prepTime >> eatTime) {
      menuItems.push_back(MenuItem(id, name, prepTime, eatTime));
    }
    file.close();
  }

  void loadMenuFromInput() {
    int n;
    cout << "Enter the number of menu items: ";
    cin >> n;
    for (int i = 0; i < n; ++i) {
      int id, prepTime, eatTime;
      string name;
      cout << "Enter ID, Name, Prep Time (min), Eating Time (min) for item "
           << i + 1 << ": ";
      cin >> id >> name >> prepTime >> eatTime;
      menuItems.push_back(MenuItem(id, name, prepTime, eatTime));
    }
  }

  void displayMenu() {
    cout << "\nMenu: \n";
    for (const auto &item : menuItems) {
      cout << item.id << ". " << item.name << " - " << item.prepTime
           << " min prep, " << item.eatTime << " min eating time.\n";
    }
  }

  MenuItem getItemById(int id) {
    for (const auto &item : menuItems) {
      if (item.id == id)
        return item;
    }
    return MenuItem(0, "Invalid", 0, 0);
  }
};

// Order Class
class Order {
public:
  int clientId;
  int itemId;
  int prepTime;
  int priority; // Lower number means higher priority

  Order(int clientId, int itemId, int prepTime, int priority)
      : clientId(clientId), itemId(itemId), prepTime(prepTime),
        priority(priority) {}

  bool operator<(const Order &other) const {
    return priority > other.priority; // Reverse order for priority queue
  }
};

// OrderQueue Class (Thread-safe with Priority)
class OrderQueue {
private:
  priority_queue<Order> orders;
  sem_t mutex; // Semaphore for mutual exclusion
  sem_t full;  // Semaphore to track full queue
  sem_t empty; // Semaphore to track empty queue

public:
  OrderQueue(int capacity) {
    sem_init(&mutex, 0, 1); // Initialize mutex to 1 (binary semaphore)
    sem_init(&full, 0, 0);  // Initialize full to 0 (queue is initially empty)
    sem_init(&empty, 0, capacity); // Initialize empty to capacity
  }

  void addOrder(const Order &order) {
    sem_wait(&empty); // Wait if the queue is full
    sem_wait(&mutex); // Enter critical section
    orders.push(order);
    cout << "Client " << order.clientId << " placed an order for Item "
         << order.itemId << " (Priority " << order.priority << ").\n";
    sem_post(&mutex); // Leave critical section
    sem_post(&full);  // Signal that the queue is not empty
  }

  Order getOrder() {
    sem_wait(&full);  // Wait if the queue is empty
    sem_wait(&mutex); // Enter critical section
    Order order = orders.top();
    orders.pop();
    sem_post(&mutex); // Leave critical section
    sem_post(&empty); // Signal that the queue is not full
    return order;
  }
};

// Server Class
class Server {
private:
  OrderQueue &orderQueue;

public:
  Server(OrderQueue &orderQueue) : orderQueue(orderQueue) {}

  static void *run(void *arg) {
    Server *server = reinterpret_cast<Server *>(arg);
    while (true) {
      Order order = server->orderQueue.getOrder();
      server->processOrder(order);
    }
    return nullptr;
  }

  void processOrder(const Order &order) {
    cout << "Server: Preparing order for Item " << order.itemId << " (Client "
         << order.clientId << ").\n";
    sleep(order.prepTime);
    cout << "Server: Delivered order for Item " << order.itemId << " (Client "
         << order.clientId << ").\n";
  }
};

// Client Class
class Client {
private:
  int clientId;
  Menu &menu;
  OrderQueue &orderQueue;

public:
  Client(int clientId, Menu &menu, OrderQueue &orderQueue)
      : clientId(clientId), menu(menu), orderQueue(orderQueue) {}

  static void *run(void *arg) {
    Client *client = reinterpret_cast<Client *>(arg);
    client->placeOrder();
    return nullptr;
  }

  void placeOrder() {
    int itemId;
    cout << "Client " << clientId
         << ": Enter the menu item ID you want to order: ";
    cin >> itemId;

    MenuItem item = menu.getItemById(itemId);
    if (item.id != 0) {
      int priority = rand() % 5 + 1; // Random priority (1 to 5)
      Order order(clientId, item.id, item.prepTime, priority);
      orderQueue.addOrder(order);
      sleep(item.eatTime);
      cout << "Client " << clientId << " finished eating.\n";
    } else {
      cout << "Client " << clientId << " chose an invalid menu item.\n";
    }
  }
};

// Main Function
int main() {
  srand(time(0));
  Menu menu;
  int choice;

  cout << "Load menu from: \n1. File\n2. Manual Input\nEnter choice: ";
  cin >> choice;

  if (choice == 1) {
    string filename;
    cout << "Enter menu file name: ";
    cin >> filename;
    menu.loadMenuFromFile(filename);
  } else {
    menu.loadMenuFromInput();
  }

  menu.displayMenu();

  int numClients, numServers, queueCapacity;
  cout << "\nEnter the number of clients: ";
  cin >> numClients;
  cout << "Enter the number of servers: ";
  cin >> numServers;
  cout << "Enter the order queue capacity: ";
  cin >> queueCapacity;

  OrderQueue orderQueue(queueCapacity);

  // Create server threads
  vector<pthread_t> serverThreads(numServers);
  for (int i = 0; i < numServers; ++i) {
    Server *server = new Server(orderQueue);
    pthread_create(&serverThreads[i], nullptr, Server::run, server);
  }

  // Create client threads
  vector<pthread_t> clientThreads(numClients);
  for (int i = 0; i < numClients; ++i) {
    Client *client = new Client(i + 1, menu, orderQueue);
    pthread_create(&clientThreads[i], nullptr, Client::run, client);
  }

  // Wait for client threads to finish
  for (auto &clientThread : clientThreads) {
    pthread_join(clientThread, nullptr);
  }

  // Terminate server threads
  for (auto &serverThread : serverThreads) {
    pthread_cancel(serverThread);
    pthread_join(serverThread, nullptr);
  }

  cout << "Restaurant is closed." << endl;
  return 0;
}
