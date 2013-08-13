#ifndef REFERENCE_H
#define REFERENCE_H

#include "vaultmp.h"
#include "CriticalSection.h"
#include "Value.h"
#include "Interface.h"
#include "RakNet.h"
#include "PacketFactory.h"

#ifdef VAULTMP_DEBUG
#include "Debug.h"
#endif

#include <queue>

/**
 * \brief The base class for all in-game types
 *
 * Data specific to References are a reference ID, a base ID and a NetworkID
 */

template<typename T>
class FactoryWrapper;

class Reference : private CriticalSection, public RakNet::NetworkIDObject
{
		friend class GameFactory;

		template<typename T>
		friend class FactoryWrapper;

	private:
#ifdef VAULTMP_DEBUG
		static DebugInput<Reference> debug;
#endif

		Value<unsigned int> refID;
		Value<unsigned int> baseID;
		Value<bool> changed;

#ifndef VAULTSERVER
		std::queue<std::function<void()>> tasks;
#endif

		Reference(const Reference&) = delete;
		Reference& operator=(const Reference&) = delete;

	protected:
		//static unsigned int ResolveIndex(unsigned int baseID);

		template <typename T>
		Lockable* SetObjectValue(Value<T>& dest, const T& value);

		Reference(unsigned int refID, unsigned int baseID);

	public:
		virtual ~Reference() noexcept;

		/**
		 * \brief Retrieves the Reference's reference ID
		 */
		unsigned int GetReference() const;
		/**
		 * \brief Retrieves the Reference's base ID
		 */
		unsigned int GetBase() const;
		/**
		 * \brief Retrieves the Reference's changed state
		 */
		bool GetChanged() const;
		/**
		 * \brief Determines if the reference ID is persistent
		 */
		bool IsPersistent() const;

		/**
		 * \brief Sets the Reference's reference ID
		 */
		Lockable* SetReference(unsigned int refID);
		/**
		 * \brief Sets the Reference's base ID
		 */
#ifdef VAULTSERVER
		virtual Lockable* SetBase(unsigned int baseID);
#else
		Lockable* SetBase(unsigned int baseID);
#endif
		/**
		 * \brief Sets the Reference's changed state
		 */
		Lockable* SetChanged(bool changed);

		/**
		 * \brief Returns a constant Parameter used to pass the reference ID of this Reference to the Interface
		 */
		RawParameter GetReferenceParam() const
		{
			return RawParameter(refID.get());
		};
		/**
		 * \brief Returns a constant Parameter used to pass the base ID of this Reference to the Interface
		 */
		RawParameter GetBaseParam() const
		{
			return RawParameter(baseID.get());
		};

#ifndef VAULTSERVER
		/**
		 * \brief Enqueues a task
		 */
		void Enqueue(const std::function<void()>& task);
		/**
		 * \brief Executes all tasks
		 */
		void Work();
		/**
		 * \brief Releases all tasks
		 */
		void Release();
#endif

		/**
		 * \brief For network transfer
		 */
		virtual pPacket toPacket() const = 0;
};

class ReferenceFunctor : public VaultFunctor
{
	private:
		unsigned int _flags;
		RakNet::NetworkID id;

	protected:
		ReferenceFunctor(unsigned int flags, RakNet::NetworkID id) : VaultFunctor(), _flags(flags), id(id) {}
		virtual ~ReferenceFunctor() {}

		virtual bool filter(FactoryWrapper<Reference>& reference) = 0;

		unsigned int flags() { return _flags; }
		RakNet::NetworkID get() { return id; }
};

#endif
