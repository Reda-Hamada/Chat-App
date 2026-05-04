#include <chrono>
#include <iostream>
#include <thread>

using namespace std;

void downloadfile() {
  cout << "Start Downloading...." << endl;
  this_thread::sleep_for(chrono::seconds(3));
  cout << "Download Complete!";
  cout << endl;
}

void multi() {
  cout << "Start calc" << endl;
  int mul = 1;
  for (int i = 1; i < 10; i++) {
    mul *= i;
    cout << mul << endl;
  }
  cout << "Finsh" << endl;
  this_thread::sleep_for(chrono::milliseconds(500));
}

void printNumber() {
  for (int i = 0; i < 5; i++) {
    cout << "Main thread work" << i << endl;
    this_thread::sleep_for(chrono::milliseconds(500));
  }
}

int main() {
  thread test(downloadfile);
  // downloadfile();
  thread test2(multi);
  printNumber();
  if (test.joinable() && test2.joinable()) {
    test.join();

    test2.join();
  }

  cout << "Main thread" << endl;

  return 0;
}
