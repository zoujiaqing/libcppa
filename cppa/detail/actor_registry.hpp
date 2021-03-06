/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_DETAIL_ACTOR_REGISTRY_HPP
#define CPPA_DETAIL_ACTOR_REGISTRY_HPP

#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <condition_variable>

#include "cppa/attachable.hpp"
#include "cppa/abstract_actor.hpp"
#include "cppa/util/shared_spinlock.hpp"

#include "cppa/detail/singleton_mixin.hpp"

namespace cppa {
namespace detail {

class singleton_manager;

class actor_registry : public singleton_mixin<actor_registry> {

    friend class singleton_mixin<actor_registry>;

 public:

    ~actor_registry();

    /**
     * @brief A registry entry consists of a pointer to the actor and an
     *        exit reason. An entry with a nullptr means the actor has finished
     *        execution for given reason.
     */
    typedef std::pair<abstract_actor_ptr, std::uint32_t> value_type;

    /**
     * @brief Returns the {nullptr, invalid_exit_reason}.
     */
    value_type get_entry(actor_id key) const;

    // return nullptr if the actor wasn't put *or* finished execution
    inline abstract_actor_ptr get(actor_id key) const {
        return get_entry(key).first;
    }

    void put(actor_id key, const abstract_actor_ptr& value);

    void erase(actor_id key, std::uint32_t reason);

    // gets the next free actor id
    actor_id next_id();

    // increases running-actors-count by one
    void inc_running();

    // decreases running-actors-count by one
    void dec_running();

    size_t running() const;

    // blocks the caller until running-actors-count becomes @p expected
    void await_running_count_equal(size_t expected);

 private:

    typedef std::map<actor_id, value_type> entries;

    std::atomic<size_t> m_running;
    std::atomic<actor_id> m_ids;

    std::mutex m_running_mtx;
    std::condition_variable m_running_cv;

    mutable util::shared_spinlock m_instances_mtx;
    entries m_entries;

    actor_registry();

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_ACTOR_REGISTRY_HPP
