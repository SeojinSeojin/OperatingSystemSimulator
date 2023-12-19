#ifndef MM_H
#define MM_H
#include "pm.h"
#include "utils.h"

class Page {
public:
  Page(int virtualAddress, int physicalAddress)
      : virtualAddress(virtualAddress), physicalAddress(physicalAddress),
        validBit(false), referenceBit(false), modifiedBit(false),
        hardDiskAddress(-1), swappedOut(false) {
    std::cout << "Page created with VA: " << virtualAddress
              << " and PA: " << physicalAddress << std::endl;
  }

  int getVirtualAddress() const { return virtualAddress; }
  int getPhysicalAddress() const { return physicalAddress; }
  bool isValid() const { return validBit; }
  bool isReferenced() const { return referenceBit; }
  bool isModified() const { return modifiedBit; }

  void setPhysicalAddress(int addr) { physicalAddress = addr; }
  void setValid(bool valid) { validBit = valid; }
  void setReferenced(bool referenced) { referenceBit = referenced; }
  void setModified(bool modified) { modifiedBit = modified; }

  int getHardDiskAddress() const { return hardDiskAddress; }
  void setHardDiskAddress(int addr) { hardDiskAddress = addr; }
  bool isSwappedOut() const { return swappedOut; }
  void setSwappedOut(bool swapped) { swappedOut = swapped; }

private:
  int virtualAddress;
  int physicalAddress;
  bool validBit;
  bool referenceBit;
  bool modifiedBit;
  int hardDiskAddress;
  bool swappedOut;
};

class PageTable {
public:
  PageTable() {}
  ~PageTable() {
    for (auto &entry : pages) {
      delete entry.second;
    }
  }

  void addPage(pid_t pid, int virtualAddress, int physicalAddress) {
    Page *page = new Page(virtualAddress, physicalAddress);
    pages.insert(std::make_pair(pid, page));
  }

  const std::map<pid_t, Page *> &getPages() const { return pages; }

  Page *getPage(pid_t pid) {
    auto it = pages.find(pid);
    if (it != pages.end()) {
      return it->second;
    }
    return nullptr;
  }

private:
  std::map<pid_t, Page *> pages;
};

class MemoryManager {
public:
  MemoryManager() : physicalMemory(MEMORY_SIZE), hardDisk(DISK_SIZE) {
    std::cout << "MemoryManager initialized with memory size: " << MEMORY_SIZE
              << " and disk size: " << DISK_SIZE << std::endl;
  }

  int getVirtualAddress(pid_t pid) {
    int virtualAddress = determineVirtualAddress(pid);
    return virtualAddress;
  }

  void writeToVirtualAddress(const std::vector<char> &data, pid_t pid) {
    Page *page = pageTable.getPage(pid);
    if (page && page->isValid()) {
      int physicalAddress = page->getPhysicalAddress();
      physicalMemory.writeToAddress(physicalAddress, data);
      page->setModified(true);
    }
  }

  void logMemoryMapping() {
    std::cout << "VA to PA Mapping:" << std::endl;
    for (const auto &ptEntry : pageTable.getPages()) {
      pid_t pid = ptEntry.first;
      const Page *page = ptEntry.second;
      std::cout << "PID: " << pid << "\t";
      int va = page->getVirtualAddress();
      int pa = page->getPhysicalAddress();
      std::cout << "VA(" << va << ") -> PA(" << pa
                << "), valid: " << (page->isValid() ? "o" : "x")
                << ", modified: " << (page->isModified() ? "o" : "x")
                << ", referenced: " << (page->isReferenced() ? "o" : "x")
                << ", hardDisk: " << page->getHardDiskAddress() << std::endl;
    }
    std::cout << "[Physical Memory]" << std::endl;
    physicalMemory.logInfo();
    std::cout << "[Hard Disk]" << std::endl;
    hardDisk.logInfo();
  }

  void logPageFaultCnt() {
    std::cout << "Total Page Fault Count: " << pageFaultCnt << std::endl;
  }

private:
  RealMemory physicalMemory;
  RealMemory hardDisk;
  PageTable pageTable;
  std::queue<size_t> fifoQueue;
  int lastUpdatedVA = 0;
  int pageFaultCnt = 0;

  int determineVirtualAddress(pid_t pid) { // 메모리에 프로세스가 붙으면 실행됨
    Page *page = pageTable.getPage(pid);
    if (page == nullptr || !page->isValid()) {
      pageFaultCnt++;
      if (page == nullptr) { // 없으면 처음으로 생성
        handlePageFault(pid, lastUpdatedVA);
        lastUpdatedVA++;
      } else { // page->isValid()가 false인 경우. 한 번 할당되었으나 swap out된 경우
        loadPageFromDisk(page);
      }
      page = pageTable.getPage(pid);
    }
    return page->getVirtualAddress();
  }

  void handlePageFault(pid_t pid, int virtualAddress) {
    std::cout << "Handling PAGE FAULT for PID: " << pid << " at VA "
              << virtualAddress << std::endl;

    if (physicalMemory.isFull()) {
      std::cout << "The PAGE FAULT reason was physical memory is full."
                << std::endl;
      swapPages();
    }

    size_t physicalAddress = physicalMemory.getFirstEmptyAddress();
    if (physicalAddress == MEMORY_SIZE) {
      swapPages();
      physicalAddress = physicalMemory.getFirstEmptyAddress();
    }

    pageTable.addPage(pid, virtualAddress, physicalAddress);
    Page *page = pageTable.getPage(pid);

    page->setValid(true);
    page->setPhysicalAddress(physicalAddress);

    fifoQueue.push(physicalAddress);
  }

  void swapPages() {
    std::cout << "Swapping pages using FIFO..." << std::endl;
    while (!fifoQueue.empty() && physicalMemory.isFull()) {
      size_t oldAddress = fifoQueue.front();
      fifoQueue.pop();
      auto pageData = physicalMemory.getValue(oldAddress);
      if (pageData.has_value()) {
        size_t diskAddress = hardDisk.getFirstEmptyAddress();
        hardDisk.writeToAddress(diskAddress, pageData.value());
        std::cout << "Swapping out PA: " << oldAddress
                  << " to Hard Disk: " << diskAddress << std::endl;
        physicalMemory.flushAddress(oldAddress);
        for (auto &entry : pageTable.getPages()) {
          Page *page = entry.second;
          if (page->getPhysicalAddress() == oldAddress &&
              page->isValid()) { // 오직 하나 존재
            std::cout << "Swapped PID: " << entry.first << std::endl;
            page->setSwappedOut(true);
            page->setValid(false);
            page->setHardDiskAddress(diskAddress);
            break;
          }
        }
      } else {
        std::cerr << "Error: Failed to get page data from physical memory"
                  << std::endl;
      }
    }
  }

  void loadPageFromDisk(Page *page) {
    if (page == nullptr) {
      std::cerr << "Error: Page is null" << std::endl;
      return;
    }

    swapPages();
    size_t newPhysicalAddress = physicalMemory.getFirstEmptyAddress();
    size_t diskAddress = page->getHardDiskAddress();
    auto pageData = hardDisk.getValue(diskAddress);

    if (pageData.has_value()) {
      std::cout << "Load page from disk: " << diskAddress
                << " to physical memory: " << newPhysicalAddress << std::endl;
      physicalMemory.writeToAddress(newPhysicalAddress, pageData.value());
      page->setSwappedOut(false);
      page->setPhysicalAddress(newPhysicalAddress);
      page->setValid(true);
      page->setHardDiskAddress(-1);
      fifoQueue.push(newPhysicalAddress);
      hardDisk.flushAddress(diskAddress);
    } else {
      std::cerr << "Error: Failed to load page data from disk" << std::endl;
    }
  }
};

#endif // MM_H