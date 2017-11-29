#pragma once

#include <stdlib.h>
#include <stdint.h>

using namespace std;

namespace sks_coll
{
	template<class Test, class Base>
	struct AssertSameOrDerivedFrom {
		AssertSameOrDerivedFrom() { &constraints; }
		static void constraints() {
			Test *pd = nullptr;
			Base *pb = pd;
		}
	};

	template<typename obtype>
	class object_array
	{
	public:
		typedef uint64_t map_idx;
		typedef uint32_t half_idx;

		const half_idx shamt      = 32;
		const map_idx  ver_iter   = static_cast<map_idx>(0x1) << shamt;
		const map_idx lo_bitmask  = 0xfffffffff;
		const map_idx hi_bitmask  = lo_bitmask << shamt;

		struct object
		{
			object() : id(0){}
			virtual ~object(){};
			virtual void enable() = 0;
			virtual void disable() = 0;	
			map_idx id;
		};

		obtype   *objs;
		half_idx *indirs;
		half_idx *free_indirs;

		half_idx count;
		half_idx maxsz;
		int64_t free_head;

		obtype* begin()
		{
			return &objs[0];
		}

		obtype* end()
		{
			return &objs[count - 1];
		}

		/**
		 * \brief Creates a new dataArray.
		 * \param max_sz Any number n where n < (uint32_t::max() - 1)
		 */
		explicit object_array(uint32_t max_sz) 
		: count (0), maxsz(max_sz), free_head(max_sz)
		{
			AssertSameOrDerivedFrom<obtype, object>();

			// allocate memory for object array, indirection array, and free indirection stack
			objs = new obtype[max_sz];
			indirs = new half_idx[max_sz];
			free_indirs = new half_idx[max_sz];

			// assign indirections 0..(n-1) + instantiate objects for object array w/ sequential IDs.
			// free indirections are assigned in reverse order, so the first element availible holds 0
			for (unsigned i = 0; i < max_sz; i++)
			{
				auto n_ob 	= new obtype();
				n_ob->id 	= i;

				indirs[i] 	= i;
				objs[i] 	= *n_ob;

				free_indirs[i] = (max_sz - i - 1);
			}
		}

		/**
		* \brief retrieves new object, returns it's key, and calls enable on that object.
		*/
		map_idx get_new()
		{
			// getnew the index of the free head, or return
			// a nullptr if there are no more free elements.
			if(free_head == std::numeric_limits<half_idx>::max() || free_head == 0)
				return reinterpret_cast<map_idx>(nullptr);

			// decrement free_head to point to the next free indirection on the free indirection array, and
			// then retrieve the index on the object array from the indirection array.
			const half_idx indir = indirs[free_indirs[--free_head]];

			// retrieve the object at the current indirection's index, and increment it's version;
			// the object already has the correct ID, as an object's ID is where it lies within the
			// indirection array. Set 'active' flag to true.
			auto ob = &objs[indir];
			ob->id += ver_iter;
			ob->enable();
			
			count++;

			return ob->id;
		}
		
		/**
		* \brief look up object by key, return by value.
		*/
		obtype& lookup(map_idx idx)
		{
			return objs[indirs[static_cast<half_idx>(idx)]];
		}

		/**
		* \brief look up object by key, return reference which might become invalidated
		* when destroying objects from the data array.
		*/
		obtype* lookup_unstable_ref(map_idx idx)
		{
			return &objs[indirs[static_cast<half_idx>(idx)]];
		}


		/**
		* \brief destroys object at passed key, if version matches. Will invalidate
		* up to one other object's pointer. Calls disable() on the object.
		*/
		void destroy(map_idx idx)
		{
			// retrieve the ref to the object being destroyed
			const auto todestr_ind_idx = static_cast<half_idx>(idx); 
			const auto todestr_arr_idx = indirs[todestr_ind_idx];
			auto obj_todestroy         = &objs[todestr_arr_idx];

			// if version does not match, return
			if (idx >> shamt != obj_todestroy->id >> shamt) return;
			obj_todestroy->disable();

			// retrieve the ref of the object to be swapped- the last element of the object
			// array. Count is decremented before retrieval.
			const auto toswap_arr_idx = --count;
			auto obj_toswap           = &objs[toswap_arr_idx];
			const auto toswap_ind_idx = static_cast<half_idx>(obj_toswap->id);

			// iterate the version of the destroyed object
			obj_todestroy->id = obj_todestroy->id + ver_iter;

			// swap objects and indirections
			swap(indirs[toswap_ind_idx], indirs[todestr_ind_idx]);
			swap(objs[toswap_arr_idx], objs[todestr_arr_idx]);

			free_indirs[free_head++] = todestr_ind_idx;
		}
	};
}
