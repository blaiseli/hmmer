//! Data structures for maintaining lists of hits in ways that make merging results from parallel threads easy
/*! How these structures work:  
 * A P7_HIT_CHUNK contains and describes a set of hits, which must be sorted in order of ascending object  ID.  
 * A P7_HITLIST contains either the entire set of hits found by a node or the entire set of hits found during a search. 
 *
 * P7_HIT_CHUNKs are typically generated by worker threads.  Worker threads search regions of a database in ascending order
 * by object ID.  When they find a hit, they add it to their P7_HIT_CHUNK.  When a worker thread finishes a region and needs to start
 * another, it inserts its chunk into the node's P7_HITLIST structure and starts a new one.  
*
* A P7_HITLIST contains a linked list of hits, sorted by object ID, and also a linked list of P7_HIT_CHUNKs, again sorted by object ID
* The P7_HIT_CHUNKs must have non-overlapping ranges of object IDs.  This will happen naturally when merging the chunks generated
* by the threads running on a single node, but merging the results from multiple machines will require merging the chunks by hand
* To insert a chunk into a hitlist, search the list of chunks in the list until you find the right place to insert the new chunk.  Splice it into 
* the list of chunks, and also splice the hits in the chunk into the full list
*/

#ifndef p7HITLIST_INCLUDED
#define p7HITLIST_INCLUDED

#include "base/p7_tophits.h"


#define HITLIST_POOL_SIZE 100 // default size of each engine's hitlist pool
//! Entry used to form a doubly-linked list of hits
/*! Invariant: hits in the list are required to be sorted in ascending order by object id  */
typedef struct p7_hitlist_entry{
	P7_HIT *hit; 
	struct p7_hitlist_entry *prev;
	struct p7_hitlist_entry *next;
} P7_HITLIST_ENTRY;

//! Structure that holds a chunk of hits, sorted by object id
typedef struct p7_hit_chunk{
	//! Beginning entry in the list
	P7_HITLIST_ENTRY *start;

	//! Last entry in the list
	P7_HITLIST_ENTRY *end;

	//! object ID of the first entry in the list
	uint64_t start_id;

	//! object ID of the last entry in the list
	uint64_t end_id;

	//Previous chunk in the list
	struct p7_hit_chunk *prev;

	//Next chunk in the list
	struct p7_hit_chunk *next;
} P7_HIT_CHUNK;

//! Holds the full list of hits that a machine has found
typedef struct p7_hitlist{
	//! lock used to serialize changes to the hitlist
	pthread_mutex_t lock;

	//! lowest-ID hit in the list
	P7_HITLIST_ENTRY *hit_list_start;

	//! highest-ID hit in the list
	P7_HITLIST_ENTRY *hit_list_end;

	//! object ID of the first entry in the list
	uint64_t hit_list_start_id;

	//! object ID of the last entry in the list
	uint64_t hit_list_end_id;

	//! Start of the list of chunks
	P7_HIT_CHUNK *chunk_list_start;

	//! End of the list of chunks
	P7_HIT_CHUNK *chunk_list_end;

} P7_HITLIST;


//Functions to create and manipulate P7_HITLIST_ENTRY objects

//! Creates a P7_HITLIST_ENTRY object and its included P7_HIT object
P7_HITLIST_ENTRY *p7_hitlist_entry_Create();


//! Creates a linked list of num_entries hitlist entries and returns it
P7_HITLIST_ENTRY *p7_hitlist_entry_pool_Create(uint32_t num_entries);

//! Destroys a P7_HITLIST_ENTRY object and its included P7_HIT object.
/*! NOTE:  do not call the base p7_hit_Destroy function on the P7_HIT object in a P7_HITLIST_ENTRY.  
 * p7_hit_Destroy calls free on some of the objects internal to the P7_HIT object.  In the hitlist, these are pointers 
 * into the daemon's data shard, so freeing them will break things very badly
 * @param the_entry the hitlist entry to be destroyed.
 */
void p7_hitlist_entry_Destroy(P7_HITLIST_ENTRY *the_entry);






//Functions to create and manipulate P7_HIT_CHUNK objects

//! create and return an empty hit chunk
P7_HIT_CHUNK * p7_hit_chunk_Create();

//! destroy a hit chunk and free its memory
/*! @param the_chunk the chunk to be destroyed */
void p7_hit_chunk_Destroy(P7_HIT_CHUNK *the_chunk);

//! adds a hitlist entry to the chunk
/*! @param  the_entry the entry to be added
 * @param the_chunk the chunk the entry should be added to
 * @return eslOK on success, fails program on failure */
int p7_add_entry_to_chunk(P7_HITLIST_ENTRY *the_entry, P7_HIT_CHUNK *the_chunk); 

//! returns a pointer to the list of hits in the chunk
/* @param the_chunk the chunk that we want the hits from */
inline P7_HITLIST_ENTRY *p7_get_hits_from_chunk(P7_HIT_CHUNK *the_chunk);


//! returns the id of the first object in the chunk's list of hits
/* @param the_chunk the hit chunk we want the start id of */
inline uint64_t p7_get_hit_chunk_start_id(P7_HIT_CHUNK *the_chunk);

//! returns the id of the last object in the chunk's list of hits
/* @param the_chunk the hit chunk we want the end id of */
inline uint64_t p7_get_hit_chunk_end_id(P7_HIT_CHUNK *the_chunk);





// Functions to create and manipolate P7_HITLIST objects

//! creates and returns a new, empty hitlist
P7_HITLIST *p7_hitlist_Create();

//! Destroys a hitlist and frees its memory
/*! @param the_list the list to be destroyed */
void p7_hitlist_Destroy(P7_HITLIST *the_list);

//! Adds a chunk to a hitlist
/*! @param the_chunk the chunk to be added
 *  @param the_list the hitlist the chunk should be added to
 *  @return eslOK on success  */
int p7_hitlist_add_Chunk(P7_HIT_CHUNK *the_chunk, P7_HITLIST *the_list);

//! Returns the length of the longest name of any of the hits in th
/*! @param th the hitlist to be searched */
uint32_t p7_hitlist_GetMaxNameLength(P7_HITLIST *th);

//! Returns the length of the longest position of any of the hits in th
/*! @param th the hitlist to be searched */
uint32_t p7_hitlist_GetMaxPositionLength(P7_HITLIST *th);

//! Returns the length of the longest accession of any of the hits in th
/*! @param th the hitlist to be searched */
uint32_t p7_hitlist_GetMaxAccessionLength(P7_HITLIST *th);

/* Function:  p7_hitlist_TabularTargets()
 * Synopsis:  Output parsable table of per-sequence hits.
 *
 * Purpose:   Output a parseable table of reportable per-sequence hits
 *            in hitlist <th> in an easily parsed ASCII
 *            tabular form to stream <ofp>.
 *            
 *            Designed to be concatenated for multiple queries and
 *            multiple top hits list.
 *
 * Returns:   <eslOK> on success.
 * 
 * Throws:    <eslEWRITE> if a write to <ofp> fails; for example, if
 *            the disk fills up.
 */
int
p7_hitlist_TabularTargets(FILE *ofp, char *qname, char *qacc, P7_HITLIST *th, double Z, int show_header);
#endif // p7HITLIST_INCLUDED
