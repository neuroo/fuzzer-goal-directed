#include <bitset>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <utility>

#include "common/logger.h"
#include "store.h"

using namespace std;

namespace instr {

Store::Store(const string &_serFileName) : serFileName(_serFileName) {
  LOG(INFO) << "Create Store";
  s = StoreImpl::fromFile(serFileName);
}

Store::~Store() {
  StoreImpl::toFile(serFileName, s);
  LOG(INFO) << "Delete Store";
}

shared_ptr<Element> Store::create(const Element::Kind kind, const element_id id,
                                  const element_id parent_id) {
  switch (kind) {
  case Element::E_SOURCE: {
    return make_shared<SourceElement>(id);
  }
  case Element::E_FUNCTION: {
    return make_shared<FunctionElement>(id, parent_id);
  }
  case Element::E_BLOCK: {
    return make_shared<BlockElement>(id, parent_id);
  }
  case Element::E_SUMMARY: {
    return make_shared<SummaryElement>(id, parent_id);
  }
  case Element::E_CONDITION: {
    return make_shared<ConditionElement>(id, parent_id);
  }
  default:
    break;
  }
  return nullptr;
}

element_id StoreImpl::addSource(const string &sourceName) {
  auto iter = sources.find(sourceName);
  if (iter != sources.end()) {
    return iter->second;
  } else {
    element_id next_id = getNextId();
    sources.emplace(make_pair(sourceName, next_id));
    return next_id;
  }
}

void StoreImpl::add(const element_id id, const shared_ptr<Element> &element) {
  if (element.get() == nullptr) {
    LOG(ERROR) << "Pointer was NULL";
    return;
  }

  LOG(INFO) << "Adding element " << element->toString() << " for id=" << id;

  if (elements.find(id) == elements.end()) {
    LOG(INFO) << "Insertion";
    elements.emplace(make_pair(id, element));
  } else {
    LOG(INFO) << "Replacement";
    elements[id] = element;
  }
}

string StoreImpl::toString() const {
  ostringstream oss;
  oss << "Store(" << endl;
  oss << " global_id := " << global_id << ", " << endl;
  oss << " sources := [" << endl;
  for (auto elmt : sources) {
    oss << elmt.first << "->" << elmt.second << ", ";
  }
  oss << endl << "]," << endl;

  oss << " elements := [" << endl;
  for (auto elmt : elements) {
    oss << elmt.first << "->" << elmt.second->toString() << ", ";
  }
  oss << endl << "]," << endl;
  oss << ");";
  return oss.str();
}

StoreImpl StoreImpl::fromFile(const string &serFileName) {
  ifstream iss(serFileName);
  if (!iss) {
    // First time, so we can create a new StoreImpl and use that one
    iss.close();
    return StoreImpl();
  } else {
    cereal::BinaryInputArchive iarchive(iss);
    StoreImpl storeImpl;
    iarchive(storeImpl);
    iss.close();
    return storeImpl;
  }
}

bool StoreImpl::toFile(const string &serFileName, const StoreImpl &s) {

  ofstream oss(serFileName, ios::binary);
  if (!oss) {
    return false;
  }
  cereal::BinaryOutputArchive oarchive(oss);
  oarchive(s);
  return true;
}
}
