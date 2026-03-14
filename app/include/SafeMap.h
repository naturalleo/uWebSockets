#ifndef UTILS_BASE_SAFE_MAP_H
#define UTILS_BASE_SAFE_MAP_H

#include <unordered_map>
#include <set>
#include <deque>


// mutex_utils.h
#include <mutex>
#ifndef MUTEX_UTILS_H
#define MUTEX_UTILS_H

#if __cplusplus >= 201703L
#include <shared_mutex>
using MutexType = std::shared_mutex;
using ConditionType = std::condition_variable_any;
using SharedLock = std::shared_lock<MutexType>;  // 共享锁
using WriteLock = std::unique_lock<MutexType>;   // 独占锁
#else
#include <mutex>
using MutexType = std::mutex;
using ConditionType = std::condition_variable;   // 退化为普通 candition
using SharedLock = std::lock_guard<MutexType>;   // 旧标准下退化为独占锁
using WriteLock = std::unique_lock<MutexType>;   // 独占锁保持不变
#endif

#endif // MUTEX_UTILS_H

template<typename K, typename V>
class SafeMap {
public:
	SafeMap() {};
	~SafeMap() {};

	SafeMap(const SafeMap& rhs) { map_ = rhs.map_; }
	SafeMap& operator=(const SafeMap& rhs)
	{
		if (&rhs != this) { map_ = rhs.map_; }
		return *this;
	};

	V& operator[](const K& key) { return map_[key]; }

	size_t size() const
	{
		SharedLock lock(mutex_);
		return map_.size();
	}

	bool empty() const
	{
		SharedLock lock(mutex_);
		return map_.empty();
	}

	bool Insert(const K& key, const V& value)
	{
		WriteLock lock(mutex_);
		auto ret = map_.insert(std::pair<K, V>(key, value));
		return ret.second;
	};

	V GetCopy(const K& key, const V& defaultValue = V{})
	{
		SharedLock lock(mutex_);
		auto iter = map_.find(key);
		if (iter == map_.end()) {
			return defaultValue;
		}
		return iter->second;
	}

	// 设置键值对，返回是否为新插入的键
	bool Set(const K& key, V&& value)
	{
		WriteLock lock(mutex_);
		auto iter = map_.find(key);
		if (iter == map_.end()) {
			map_[key] = std::move(value);
			return true;  // 新插入的键
		} else {
			iter->second = std::move(value);
			return false; // 更新已存在的键
		}
	}

	void emplace(const K& key, V&& value)
	{
		WriteLock lock(mutex_);
		map_.emplace(key, std::move(value) );
	};

	// 优化EnsureInsert方法，直接更新值而不是删除再插入
	void EnsureInsert(const K& key, const V& value) {
		WriteLock lock(mutex_);
		auto ret = map_.insert(std::pair<K, V>(key, value));
		if (!ret.second) {
			ret.first->second = value;
		}
	};

	std::set<K> GetKeys() const
	{
		std::set<K> res;
		SharedLock lock(mutex_); // 使用shared_lock进行读操作
		for (auto& kv : map_)
			res.emplace(kv.first);
		return std::move(res);
	};

	// 找到返回 true，找不到返回 false 
	bool FindAndRef(const K& key, OUT V& value) {
		SharedLock lock(mutex_); // 使用shared_lock进行读操作
		auto iter = map_.find(key);
		if (iter == map_.end()) return false;
		value = iter->second;
		return true;
	};

    // 使用传入的比较方法做比较, 比较函数返回 true 则表示匹配上，使用 out 传出去
	bool FindAndRef(const std::function<bool(const K&, const V&)>& comparefun,OUT V& value) {
		SharedLock lock(mutex_); // 使用shared_lock进行读操作
		for (auto iter = map_.begin(); iter != map_.end(); iter++)
		{
			if (comparefun(iter->first, iter->second))
			{
				value = iter->second;
				return true;
			}
		}
		return false;
	};

	bool ContainsKey(const K& key) const
	{
		SharedLock lock(mutex_); // 使用shared_lock进行读操作
		return map_.find(key) != map_.end();
	};

	bool TryRemove(const K& key, OUT V& value) {
		WriteLock lock(mutex_);
		auto iter = map_.find(key);
		if (iter == map_.end())  return false;
		value = std::move(iter->second);
		map_.erase(iter);
		return true;
	};

	void Erase(const K& key) {
		WriteLock lock(mutex_);
		map_.erase(key);
	};

	std::unique_ptr<V> TryGetAndErase(const K& key)
	{
		WriteLock lock(mutex_);
		auto iter = map_.find(key);
		if (iter == map_.end())  return false;
		std::unique_ptr<V> valuePtr(new V(std::move(iter->second)) );  // 移动到unique_ptr;
		//value = std::move(iter->second);
		map_.erase(iter);
		return valuePtr;
	}

	void Clear() {
		WriteLock lock(mutex_);
		map_.clear();
	}

	// 遍历, 可以直接修改 Value , 不建议其他复杂逻辑，避免锁
	void Foreach(const std::function<void(const K&, V&)>& callback) {
		WriteLock lock(mutex_);
		for (auto& kv : map_) {
			callback(kv.first, kv.second);
		}
	}

	// 只读遍历 全都声明为  const , 
	// callback  return false 则退出循环， true 继续循环
	void ForeachReadyOnly(const std::function<bool(const K&, const V&)>& callback) const {
		SharedLock lock(mutex_);
		for (const auto& kv : map_) {
			if ( !callback(kv.first, kv.second)) break;
		}
	}

	// 使用 shouldErase 判断是否需要删除元素
	bool EraseIf(const std::function<bool(const K&, const V&)>& shouldErase) {
		WriteLock lock(mutex_);
		for (auto it = map_.begin(); it != map_.end();) {
			if (shouldErase(it->first, it->second)) it = map_.erase(it);
			else                                  ++it;
		}
		return !map_.empty(); // 返回是否还有剩余元素
	}

private:
	mutable MutexType mutex_;
	std::unordered_map<K, V> map_;
};




template<typename T>
class SafeSet {
public:
	SafeSet() {};
	~SafeSet() {};

	SafeSet(const SafeSet& rhs) { set_ = rhs.set_; }
	SafeSet& operator=(const SafeSet& rhs)
	{
		if (&rhs != this) { set_ = rhs.set_; }
		return *this;
	};

	int size() const
	{
		SharedLock lock(mutex_);
		return set_.size();
	}

	bool empty() const
	{
		SharedLock lock(mutex_);
		return set_.empty();
	}

	bool Insert(const T& value)
	{
		WriteLock lock(mutex_);
		auto ret = set_.insert(value);
		return ret.second;
	};

	bool Contains(const T& value) const
	{
		SharedLock lock(mutex_);
		return set_.find(value) != set_.end();
	};

	bool TryRemove(const T& value) {
		WriteLock lock(mutex_);
		auto iter = set_.find(value);
		if (iter == set_.end())  return false;
		set_.erase(iter);
		return true;
	};

	void Erase(const T& value) {
		WriteLock lock(mutex_);
		set_.erase(value);
	};

	void Clear() {
		WriteLock lock(mutex_);
		set_.clear();
	};

	std::set<T> GetCopy() {
		std::set<T> tmp;
		SharedLock lock(mutex_);
		for (auto T : set_)
			tmp.insert(T);
		return std::move(tmp);
	};


private:
	mutable MutexType mutex_;
	std::set<T> set_;
};



// MT4 交易专用 Key to  FIFO queue
template<typename K, typename V>
class SafeMapToQueue {
public:
	SafeMapToQueue() = default;
	~SafeMapToQueue() = default;

	// 禁用拷贝操作（防止意外拷贝）
	SafeMapToQueue(const SafeMapToQueue&) = delete;
	SafeMapToQueue& operator=(const SafeMapToQueue&) = delete;

	// 使用右值引用参数，强制移动语义
	void PushToQueue(const K& key, V&& value) {
		WriteLock lock(mutex_);
		auto& queue = map_[key];
		queue.push_back(std::move(value));  // 移动插入
	};

	// 返回std::unique_ptr表示成功/失败
	std::unique_ptr<V> TryPopFront(const K& key) {
		WriteLock lock(mutex_);
		auto it = map_.find(key);
		if (it == map_.end() || it->second.empty()) {
			return nullptr;  // 失败返回空指针
		}
		std::unique_ptr<V> valuePtr(new V(std::move(it->second.front())));  // 移动到unique_ptr
		it->second.pop_front();
		if (it->second.empty()) {
			map_.erase(it);  // 队列空则删除键
		}
		return valuePtr;
	};

	// 可选：检查队列是否为空
	bool IsQueueEmpty(const K& key) const {
		SharedLock lock(mutex_);
		auto it = map_.find(key);
		return it == map_.end() || it->second.empty();
	};

private:
	mutable MutexType mutex_;
	std::unordered_map<K, std::deque<V>> map_;
};

//
//template<typename T>
//class SafeQueue {
//private:
//	MutexType     _mtx;  // 读写锁
//	std::queue<T> _queue;
//	ConditionType _cv;
//
//public:
//	SafeQueue() = default;
//
//	// 拷贝构造函数（允许多读并发）
//	SafeQueue(const SafeQueue& other) {
//		WriteLock lock(other._mtx);  // 共享锁
//		_queue = other._queue;
//	}
//
//	// 清空队列（独占操作）
//	void clear() {
//		WriteLock lock(_mtx);
//		while (!_queue.empty()) _queue.pop();
//	}
//
//	// 入队操作（右值/左值引用）
//	void push(T& new_value) {
//		WriteLock lock(_mtx);
//		_queue.push(std::move(new_value));
//		_cv.notify_one();
//	}
//
//	void push(T&& new_value) {
//		WriteLock lock(_mtx);
//		_queue.push(std::move(new_value));
//		_cv.notify_one();
//	}
//
//	// 等待并弹出元素（独占锁）
//	void wait_and_pop(T& value) {
//		WriteLock lock(_mtx);
//		_cv.wait(lock, [this] { return !_queue.empty(); });
//		value = _queue.front();
//		_queue.pop();
//	}
//
//	// 带超时的等待（返回智能指针）
//	std::shared_ptr<T> wait_value_for(int timeout_ms) {
//		WriteLock lock(_mtx);
//		if (_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
//			[this] { return !_queue.empty(); })) {
//			auto res = std::make_shared<T>(_queue.front());
//			_queue.pop();
//			return res;
//		}
//		return nullptr;
//	}
//
//	// 带超时的等待（返回bool）
//	bool wait_for_value(std::chrono::milliseconds ms, T& value) {
//		WriteLock lock(_mtx);
//		if (_cv.wait_for(lock, ms, [this] { return !_queue.empty(); })) {
//			value = _queue.front();
//			_queue.pop();
//			return true;
//		}
//		return false;
//	}
//
//	// 手动弹出（独占操作）
//	void pop() {
//		WriteLock lock(_mtx);
//		_queue.pop();
//	}
//
//	// 等待并弹出（无超时）
//	std::shared_ptr<T> wait_and_pop() {
//		WriteLock lock(_mtx);
//		_cv.wait(lock, [this] { return !_queue.empty(); });
//		auto res = std::make_shared<T>(_queue.front());
//		_queue.pop();
//		return res;
//	}
//
//	// 尝试弹出（立即返回）
//	bool try_pop(T& value) {
//		{
//			SharedLock lock(_mtx);  // 共享锁检查是否为空
//			if (_queue.empty()) return false;
//		}
//		// 升级为独占锁执行弹出
//		WriteLock unique_lock(std::move(lock));
//		value = _queue.front();
//		_queue.pop();
//		return true;
//	}
//
//	std::shared_ptr<T> try_pop() {
//		{
//			SharedLock lock(_mtx);  // 共享锁检查是否为空
//			if (_queue.empty()) return false;
//		}
//		// 升级为独占锁执行弹出
//		WriteLock unique_lock(std::move(lock));
//		_queue.pop();
//		return res;
//	}
//
//	// 只读接口（共享锁）
//	bool empty() const {
//		SharedLock lock(_mtx);
//		return _queue.empty();
//	}
//
//	int size() const {
//		SharedLock lock(_mtx);
//		return _queue.size();
//	}
//};
//


#endif