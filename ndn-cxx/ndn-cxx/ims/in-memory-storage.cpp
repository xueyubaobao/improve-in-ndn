/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2019 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#include "ndn-cxx/ims/in-memory-storage.hpp"
#include "ndn-cxx/ims/in-memory-storage-entry.hpp"

namespace ndn {

const time::milliseconds InMemoryStorage::INFINITE_WINDOW(-1);
const time::milliseconds InMemoryStorage::ZERO_WINDOW(0);

InMemoryStorage::const_iterator::const_iterator(const Data* ptr, const Cache* cache,
                                                Cache::index<byHashCode>::type::iterator it)
  : m_ptr(ptr)
  , m_cache(cache)
  , m_it(it)
{
}
InMemoryStorage::const_iterator&
InMemoryStorage::const_iterator::operator++()
{
  m_it++;
  if (m_it != m_cache->get<byHashCode>().end()) {
    m_ptr = &((*m_it)->getData());
  }
  else {
    m_ptr = nullptr;
  }

  return *this;
}

InMemoryStorage::const_iterator
InMemoryStorage::const_iterator::operator++(int)
{
  InMemoryStorage::const_iterator i(*this);
  this->operator++();
  return i;
}

InMemoryStorage::const_iterator::reference
InMemoryStorage::const_iterator::operator*()
{
  return *m_ptr;
}

InMemoryStorage::const_iterator::pointer
InMemoryStorage::const_iterator::operator->()
{
  return m_ptr;
}

bool
InMemoryStorage::const_iterator::operator==(const const_iterator& rhs)
{
  return m_it == rhs.m_it;
}

bool
InMemoryStorage::const_iterator::operator!=(const const_iterator& rhs)
{
  return m_it != rhs.m_it;
}

InMemoryStorage::InMemoryStorage(size_t limit)
  : m_limit(limit)
  , m_nPackets(0)
{
  init();
}

InMemoryStorage::InMemoryStorage(boost::asio::io_service& ioService, size_t limit)
  : m_limit(limit)
  , m_nPackets(0)
{
  m_scheduler = make_unique<Scheduler>(ioService);
  init();
}

void
InMemoryStorage::init()
{
  // TODO consider a more suitable initial value
  m_capacity = m_initCapacity;

  if (m_limit != std::numeric_limits<size_t>::max() && m_capacity > m_limit) {
    m_capacity = m_limit;
  }

  for (size_t i = 0; i < m_capacity; i++) {
    m_freeEntries.push(new InMemoryStorageEntry());
  }
}

InMemoryStorage::~InMemoryStorage()
{
  // evict all items from cache
  Cache::iterator it = m_cache.begin();
  while (it != m_cache.end()) {
    it = freeEntry(it);
  }

  BOOST_ASSERT(m_freeEntries.size() == m_capacity);

  while (!m_freeEntries.empty()) {
    delete m_freeEntries.top();
    m_freeEntries.pop();
  }
}

void
InMemoryStorage::setCapacity(size_t capacity)
{
  size_t oldCapacity = m_capacity;
  m_capacity = std::max(capacity, m_initCapacity);

  if (size() > m_capacity) {
    ssize_t nAllowedFailures = size() - m_capacity;
    while (size() > m_capacity) {
      if (!evictItem() && --nAllowedFailures < 0) {
        NDN_THROW(Error());
      }
    }
  }

  if (m_capacity >= oldCapacity) {
    for (size_t i = oldCapacity; i < m_capacity; i++) {
      m_freeEntries.push(new InMemoryStorageEntry());
    }
  }
  else {
    for (size_t i = oldCapacity; i > m_capacity; i--) {
      delete m_freeEntries.top();
      m_freeEntries.pop();
    }
  }

  BOOST_ASSERT(size() + m_freeEntries.size() == m_capacity);
}

void
InMemoryStorage::insert(const Data& data,const time::milliseconds& mustBeFreshProcessingWindow)
{
  printf("----------------------InMemoryStorage::Insert-----------------------");
 // check if identical hashCode exists
   auto it = m_cache.get<byHashCode>().find(data.getHash());
   if (it != m_cache.get<byHashCode>().end())
      return;
  //if full, double the capacity
  bool doesReachLimit = (getLimit() == getCapacity());
  if (isFull() && !doesReachLimit) {
    // note: This is incorrect if 2*capacity overflows, but memory should run out before that
    size_t newCapacity = std::min(2 * getCapacity(), getLimit());
    setCapacity(newCapacity);
  }

  //if full and reach limitation of the capacity, employ replacement policy
  if (isFull() && doesReachLimit) {
    evictItem();
  }

  //insert to cache
  BOOST_ASSERT(m_freeEntries.size() > 0);
  // take entry for the memory pool
  InMemoryStorageEntry* entry = m_freeEntries.top();
  m_freeEntries.pop();
  m_nPackets++;
  entry->setData(data,data.getHash());
  if (m_scheduler != nullptr && mustBeFreshProcessingWindow > ZERO_WINDOW) {
    entry->scheduleMarkStale(*m_scheduler, mustBeFreshProcessingWindow);
  }
  m_cache.insert(entry);

  //let derived class do something with the entry
  afterInsert(entry);
}

//find function is used to find in cs not PIT
shared_ptr<const Data>
InMemoryStorage::find(const std::string hashCode)
{
   printf("----------------------InMemoryStorage::find(hashCode)-----------------------");

  auto it = m_cache.get<byHashCode>().lower_bound(hashCode);
  if(it == m_cache.get<byHashCode>().end()){
    return nullptr;
    }
  afterAccess(*it);
//  return ((*it)->getData()).shared_from_this();
 return  ((*it)->getData()).shared_from_this();//if hashCode equals, return the hash
}
//find in cs
shared_ptr<const Data>
InMemoryStorage::find(const Interest& interest)
{
  // if the interest contains implicit digest, it is possible to directly locate a packet.
  auto it = m_cache.get<byHashCode>().find(interest.getHashCode());
  printf("----------------------InMemoryStorage::find(Interest)-----------------------");
  // if a packet is located by its full name, it must be the packet to return.
  if (it == m_cache.get<byHashCode>().end()) {
    return nullptr;
  }
  return ((*it)->getData()).shared_from_this();
}

InMemoryStorage::Cache::iterator
InMemoryStorage::freeEntry(Cache::iterator it)
{
  // push the *empty* entry into mem pool
  (*it)->release();
  m_freeEntries.push(*it);
  m_nPackets--;
  return m_cache.erase(it);
}

void
InMemoryStorage::erase(const std::string hashCode)
{
          printf("----------------------InMemoryStorage::erase(%s)-----------------------",hashCode.c_str());
    auto it = m_cache.get<byHashCode>().find(hashCode);
    if (it == m_cache.get<byHashCode>().end())
      return;

    // let derived class do something with the entry
    beforeErase(*it);
    freeEntry(it);

  if (m_freeEntries.size() > (2 * size()))
    setCapacity(getCapacity() / 2);
}

void
InMemoryStorage::eraseImpl(const std::string hashCode)
{
  auto it = m_cache.get<byHashCode>().find(hashCode);
  if (it == m_cache.get<byHashCode>().end())
    return;

  freeEntry(it);
}

InMemoryStorage::const_iterator
InMemoryStorage::begin() const
{
  auto it = m_cache.get<byHashCode>().begin();
  return const_iterator(&((*it)->getData()), &m_cache, it);
}

InMemoryStorage::const_iterator
InMemoryStorage::end() const
{
  auto it = m_cache.get<byHashCode>().end();
  return const_iterator(nullptr, &m_cache, it);
}

void
InMemoryStorage::afterInsert(InMemoryStorageEntry* entry)
{
}

void
InMemoryStorage::beforeErase(InMemoryStorageEntry* entry)
{
}

void
InMemoryStorage::afterAccess(InMemoryStorageEntry* entry)
{
}

void
InMemoryStorage::printCache(std::ostream& os) const
{
  // start from the upper layer towards bottom
  for (const auto& elem : m_cache.get<byHashCode>())
    os << elem->getHash() << std::endl;
}

} // namespace ndn