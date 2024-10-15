#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <iterator>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::move(other.buffer_);
        capacity_ = std::move(other.capacity_);
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            capacity_ = 0;
            Swap(rhs);

        }
        return *this;
    }
    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector crhs(rhs);
                Swap(crhs);
            }
            else {
                size_t new_size = std::min(size_, rhs.size_);
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + new_size, data_.GetAddress());

                if (rhs.size_ < size_) {
                    std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
                }
                else if (rhs.size_ > size_) {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector(Vector&& rhs) noexcept {
        Swap(rhs);
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }


    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }
    template <typename Total>
    void PushBack(Total&& value) {
        EmplaceBack(std::forward<Total>(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* res = nullptr;
        if (size_ < data_.Capacity()) {
            res = new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
        }
        else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            res = new (new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress() + size_, 1);
                    throw;
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
        }
        return *res;
    }

    void PopBack() /* noexcept */ {
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (pos == end()) return &EmplaceBack(std::forward<Args&&>(args)...);
        size_t index = pos - begin();
        if (size_ < data_.Capacity()) {
            T tmp(std::forward<Args>(args)...);
            new (end()) T(std::forward<T>(*(end() - 1)));
            try {
                std::move_backward(begin() + index, end() - 1, end());
            }
            catch (...) {
                std::destroy_n(end(), 1);
                throw;
            }
            data_[index] = std::move(tmp);
        }
        else {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                ReallocateWithMove(index, std::forward<Args>(args)...);
            }
            else {
                ReallocateWithCopy(index, std::forward<Args>(args)...);
            }
        }
        ++size_;
        return begin() + index;
    }

    template <typename... Args>
    void ReallocateWithCopy(size_t index, Args&&... args) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data + index) T(std::forward<Args>(args)...);

        try {
            std::uninitialized_copy_n(begin(), index, new_data.GetAddress());
            std::uninitialized_copy_n(begin() + index, size_ - index, new_data.GetAddress() + index + 1);
        }
        catch (...) {
            std::destroy_n(new_data.GetAddress() + index, 1);
            throw;
        }

        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

    template <typename... Args>
    void ReallocateWithMove(size_t index, Args&&... args) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data + index) T(std::forward<Args>(args)...);

        std::uninitialized_move_n(begin(), index, new_data.GetAddress());
        std::uninitialized_move_n(begin() + index, size_ - index, new_data.GetAddress() + index + 1);

        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        iterator it_pos = const_cast<iterator>(pos);
        std::move(it_pos + 1, end(), it_pos);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
        return it_pos;
    }
    iterator Insert(const_iterator pos, const T& value) { return Emplace(pos, value); }
    iterator Insert(const_iterator pos, T&& value) { return Emplace(pos, std::move(value)); }

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return cbegin();
    }
    const_iterator end() const noexcept {
        return cend();
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};