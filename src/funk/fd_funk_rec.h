#ifndef HEADER_fd_src_funk_fd_funk_rec_h
#define HEADER_fd_src_funk_fd_funk_rec_h

/* This provides APIs for managing funk records.  It is generally not
   meant to be included directly.  Use fd_funk.h instead. */

#include "fd_funk_txn.h" /* Includes fd_funk_base.h */

/* FD_FUNK_REC_FLAG_* are flags that can be bit-ored together to specify
   how records are to be interpreted.

   - ERASE indicates a record in an in-preparation transaction should be
   erased if and when the in-preparation transaction is published.  If
   set, there will be no value resources used by this record.  Will not
   be set on a published record.  Will not be set if an in-preparation
   transaction ancestor has this record with erase set.  If set, the
   first ancestor transaction encountered (going from youngest to
   oldest) will not have erased set. */

#define FD_FUNK_REC_FLAG_ERASE (1UL<<0)

/* FD_FUNK_REC_IDX_NULL gives the map record idx value used to represent
   NULL.  This value also set a limit on how large rec_max can be. */

#define FD_FUNK_REC_IDX_NULL (ULONG_MAX)

/* A fd_funk_rec_t describes a funk record. */

struct fd_funk_rec {

  /* These fields are managed by the funk's rec_map */

  fd_funk_xid_key_pair_t pair;     /* Transaction id and record key pair */
  ulong                  map_next; /* Internal use by map */

  /* These fields are managed by funk */

  ulong prev_idx; /* Record map index of previous record */
  ulong next_idx; /* Record map index of next record */
  uint  txn_cidx; /* Compressed transaction map index (or compressed FD_FUNK_TXN_IDX if this is in the last published) */
  uint  tag;      /* Internal use only */
  ulong flags;    /* Flags that indicate how to interpret a record */

  /* Record val metadata goes here */
};

typedef struct fd_funk_rec fd_funk_rec_t;

/* fd_funk_rec_map allows for indexing records by their (xid,key) pair.
   It is used to store all records of the last published transaction and
   the records being updated for a transaction that is in-preparation.
   Published records are stored under the pair (root,key).  (This is
   done so that publishing a transaction doesn't require updating all
   transaction id of all the records that were not updated by the
   publish.) */

#define MAP_NAME              fd_funk_rec_map
#define MAP_T                 fd_funk_rec_t
#define MAP_KEY_T             fd_funk_xid_key_pair_t
#define MAP_KEY               pair
#define MAP_KEY_EQ(k0,k1)     fd_funk_xid_key_pair_eq((k0),(k1))
#define MAP_KEY_HASH(k0,seed) fd_funk_xid_key_pair_hash((k0),(seed))
#define MAP_KEY_COPY(kd,ks)   fd_funk_xid_key_pair_copy((kd),(ks))
#define MAP_NEXT              map_next
#define MAP_MAGIC             (0xf173da2ce77ecdb0UL) /* Firedancer rec db version 0 */
#define MAP_IMPL_STYLE        1
#include "../util/tmpl/fd_map_giant.c"

FD_PROTOTYPES_BEGIN

/* fd_funk_rec_idx_is_null returns 1 if idx is FD_FUNK_REC_IDX_NULL and
   0 otherwise. */

FD_FN_CONST static inline int fd_funk_rec_idx_is_null( ulong idx ) { return idx==FD_FUNK_REC_IDX_NULL; }

/* Accessors */

/* fd_funk_rec_cnt returns the number of records in the record map.
   Assumes map==fd_funk_rec_map( funk, fd_funk_wksp( funk ) ) where funk
   is a current local join.  See fd_funk.h for fd_funk_rec_max. */

FD_FN_PURE static inline ulong fd_funk_rec_cnt( fd_funk_rec_t const * map ) { return fd_funk_rec_map_key_cnt( map ); }

/* fd_funk_rec_is_full returns 1 if the record map is full (i.e. the
   maximum of records that can be concurrently tracked has been reached)
   and 0 otherwise.  Note that this includes all the records in the last
   published transactions and records being updated by in-preparation
   transactions.  Assumes funk is a current local join and
   map==fd_funk_rec_map( funk, fd_funk_wksp( funk ) ). */

FD_FN_PURE static inline int fd_funk_rec_is_full( fd_funk_rec_t const * map ) { return fd_funk_rec_map_is_full( map ); }

/* fd_funk_rec_query queries the in-preparation transaction pointed to
   by txn for the record whose key matches the key pointed to by key.
   If txn is NULL, the query will be done for the funk's last published
   transaction.  Returns a pointer to current record on success and NULL
   on failure.  Reasons for failure include txn is neither NULL nor a
   pointer to a in-preparation transaction, key is NULL or not a record
   in the given transaction.

   The returned pointer is in the caller's address space and, if the
   return value is non-NULL, the lifetime of the returned pointer is the
   lesser of the current local join, the key is removed from the
   transaction, the lifetime of the in-preparation transaction (txn is
   non-NULL) or the next successful publication (txn is NULL).

   Assumes funk is a current local join (NULL returns NULL), txn is NULL
   or points to an in-preparation transaction in the caller's address
   space, key points to a record key in the caller's address space (NULL
   returns NULL), and no concurrent operations on funk, txn or key.
   funk retains no interest in key.  The funk retains ownership of any
   returned record.  The record value metadata will be updated whenever
   the record value modified.

   fd_funk_rec_query_const is the same but it is safe to have multiple
   threads concurrently run queries.

   These are a reasonably fast O(1).

   fd_funk_rec_query_global is the same but will query txn's ancestors
   for key from youngest to oldest if key is not part of txn.  As such,
   the txn of the returned record may not match txn but will be the txn
   of most recent ancestor with the key otherwise.

   fd_funk_rec_query_global_const is the same but it is safe to have
   multiple threads concurrently run queries.

   Important safety tip!  These functions can potentially return records
   that have the ERASE flag set.  (This allows, for example, a caller to
   discard an erase for an unfrozen in-preparation transaction.)  In
   such cases, the record will have no value resources in use.

   These are a reasonably fast O(in_prep_ancestor_cnt). */

FD_FN_PURE fd_funk_rec_t const *
fd_funk_rec_query( fd_funk_t *               funk,
                   fd_funk_txn_t const *     txn,
                   fd_funk_rec_key_t const * key );

FD_FN_PURE fd_funk_rec_t const *
fd_funk_rec_query_const( fd_funk_t *               funk,
                         fd_funk_txn_t const *     txn,
                         fd_funk_rec_key_t const * key );

FD_FN_PURE fd_funk_rec_t const *
fd_funk_rec_query_global( fd_funk_t *               funk,
                          fd_funk_txn_t const *     txn,
                          fd_funk_rec_key_t const * key );

FD_FN_PURE fd_funk_rec_t const *
fd_funk_rec_query_global_const( fd_funk_t *               funk,
                                fd_funk_txn_t const *     txn,
                                fd_funk_rec_key_t const * key );

/* fd_funk_rec_test tests the record pointed to by rec.  Returns
   FD_FUNK_SUCCESS (0) if rec appears to be a live unfrozen record in
   funk and a FD_FUNK_ERR_* (negative) otherwise.  Specifically:

     FD_FUNK_ERR_INVAL - bad inputs (NULL funk, NULL rec, rec is clearly
       not from funk, etc)

     FD_FUNK_ERR_KEY - the record did not appear to be a live record.
       Specifically rec's (xid,key) did not resolve to to itself.

     FD_FUNK_ERR_XID - memory corruption was detected in testing rec

     FD_FUNK_ERR_FROZEN - rec is part of a frozen transaction

   If fd_funk_rec_test returns SUCCESS, modify and remove are guaranteed
   to succeed immediately after return.  The value returned by test will
   stable for the same lifetime as a modify.

   Assumes funk is a current local join (NULL returns NULL).

   This is a reasonably fast O(1). */

FD_FN_PURE int
fd_funk_rec_test( fd_funk_t *           funk,
                  fd_funk_rec_t const * rec );

/* fd_funk_rec_{pair,xid,key} returns a pointer in the local address
   space of the {(transaction id,record key) pair,transaction id,record
   key} of a live record.  Assumes rec points to a live record in the
   caller's address space.  The lifetime of the returned pointer is the
   same as rec.  The value at the pointer will be constant for its
   lifetime. */

FD_FN_CONST static inline fd_funk_xid_key_pair_t const * fd_funk_rec_pair( fd_funk_rec_t const * rec ) { return &rec->pair;    }
FD_FN_CONST static inline fd_funk_txn_xid_t const *      fd_funk_rec_xid ( fd_funk_rec_t const * rec ) { return rec->pair.xid; }
FD_FN_CONST static inline fd_funk_rec_key_t const *      fd_funk_rec_key ( fd_funk_rec_t const * rec ) { return rec->pair.key; }

/* fd_funk_rec_txn returns the in-preparation transaction to which the
   live record rec belongs or NULL if rec belongs to the last published
   transaction.

   fd_funk_rec_{next,prev} returns the {next,prev} record
   ({younger,older} record) of the set of records to which rec belongs
   or NULL if rec is the {youngest,oldest}.

   fd_funk_txn_rec_{head,tail} returns the {head,tail} record
   ({oldest,youngest} record) of the in-preparation transaction to which
   rec belongs or NULL if rec_next is the {youngest,oldest} in that
   transaction.

   All pointers are in the caller's address space.  These are all a fast
   O(1) but not fortified against memory data corruption. */

FD_FN_PURE static inline fd_funk_txn_t const *     /* Lifetime as described in fd_funk_txn_query */
fd_funk_rec_txn( fd_funk_rec_t const * rec,        /* Assumes live funk record, funk current local join */
                 fd_funk_txn_t const * txn_map ) { /* Assumes == fd_funk_txn_map( funk, fd_funk_wksp( funk ) ) */
  ulong txn_idx = fd_funk_txn_idx( rec->txn_cidx );
  if( fd_funk_txn_idx_is_null( txn_idx ) ) return NULL; /* TODO: consider branchless */
  return txn_map + txn_idx;
}

FD_FN_PURE static inline fd_funk_rec_t const *      /* Lifetime as described in fd_funk_rec_query */
fd_funk_rec_next( fd_funk_rec_t const * rec,        /* Assumes live funk record, funk current local join */
                  fd_funk_rec_t const * rec_map ) { /* Assumes == fd_funk_rec_map( funk, fd_funk_wksp( funk ) ) */
  ulong rec_idx = rec->next_idx;
  if( fd_funk_rec_idx_is_null( rec_idx ) ) return NULL; /* TODO: consider branchless */
  return rec_map + rec_idx;
}

FD_FN_PURE static inline fd_funk_rec_t const *      /* Lifetime as described in fd_funk_rec_query */
fd_funk_rec_prev( fd_funk_rec_t const * rec,        /* Assumes live funk record, funk current local join */
                  fd_funk_rec_t const * rec_map ) { /* Assumes == fd_funk_rec_map( funk, fd_funk_wksp( funk ) ) */
  ulong rec_idx = rec->prev_idx;
  if( fd_funk_rec_idx_is_null( rec_idx ) ) return NULL; /* TODO: consider branchless */
  return rec_map + rec_idx;
}

FD_FN_PURE static inline fd_funk_rec_t const *          /* Lifetime as described in fd_funk_rec_query */
fd_funk_txn_rec_head( fd_funk_txn_t const * txn,        /* Assumes an in-preparation transaction, funk current local join */
                      fd_funk_rec_t const * rec_map ) { /* Assumes == fd_funk_rec_map( funk, fd_funk_wksp( funk ) ) */
  ulong rec_head_idx = txn->rec_head_idx;
  if( fd_funk_rec_idx_is_null( rec_head_idx ) ) return NULL; /* TODO: consider branchless */
  return rec_map + rec_head_idx;
}

FD_FN_PURE static inline fd_funk_rec_t const *          /* Lifetime as described in fd_funk_rec_query */
fd_funk_txn_rec_tail( fd_funk_txn_t const * txn,        /* Assumes an in-preparation transaction, funk current local join */
                      fd_funk_rec_t const * rec_map ) { /* Assumes == fd_funk_rec_map( funk, fd_funk_wksp( funk ) ) */
  ulong rec_tail_idx = txn->rec_tail_idx;
  if( fd_funk_rec_idx_is_null( rec_tail_idx ) ) return NULL; /* TODO: consider branchless */
  return rec_map + rec_tail_idx;
}

/* fd_funk_rec_modify returns rec as a non-const rec if rec is currently
   safe to modify the val / discard a change to the record for an
   in-preparation transaction (incl discard an erase) / erase a
   published record / etc.  Reasons for NULL include NULL funk, NULL
   rec, rec does not appear to be a live record, or the transaction to
   which rec belongs is frozen.

   The returned pointer is in the caller's address space and, if the
   return value is non-NULL, the lifetime of the returned pointer is the
   the lesser of the current local join, the key has been removed from
   the transaction, the transaction becomes frozen due to birth of an
   in-preparation child, (for a key part of an in-preparation
   transaction) the lifetime of in-preparation transaction, (for a key
   part of the last published transaction) the next successful
   publication.

   Assumes funk is a current local join (NULL returns NULL), rec is a
   pointer in the caller's address space to a fd_funk_rec_t (NULL
   returns NULL), and no concurrent operations on funk or rec.  The funk
   retains ownership of rec.  The record value metadata will be updated
   whenever the record value modified.

   This is a reasonably fast O(1). */

FD_FN_PURE fd_funk_rec_t *
fd_funk_rec_modify( fd_funk_t *           funk,
                    fd_funk_rec_t const * rec );

/* fd_funk_rec_insert inserts a new record whose key will be the same
   as the record key pointed to by key to the in-preparation transaction
   pointed to by txn.  If txn is NULL, the record will be inserted into
   the last published transaction.

   Returns a pointer to the new record on success and NULL on failure.
   If opt_err is non-NULL, on return, *opt_err will indicate the result
   of the operation.

     FD_FUNK_SUCCESS - success

     FD_FUNK_ERR_INVAL - failed due to bad inputs (NULL funk, NULL key,
       txn was neither NULL nor pointer to an in-preparation
       transaction)

     FD_FUNK_ERR_REC - failed due to too many records in the func,
       increase rec_max

     FD_FUNK_ERR_FROZEN - txn is a transaction that is a parent of an
       in-preparation transaction.

     FD_FUNK_ERR_KEY - key referred to an record that is already present
       in the transaction.

   The returned pointer is in the caller's address space and, if the
   return value is non-NULL, the lifetime of the returned pointer is the
   lesser of the current local join, the record is removed, the txn's
   lifetime (only applicable if txn is non-NULL) or the next successful
   publication (only applicable if txn is NULL).

   Note, if this insert is for a record that in txn with the ERASE flag
   set, the ERASE flag of the record will be cleared and it will return
   that record.

   Assumes funk is a current local join (NULL returns NULL), txn is NULL
   or points to an in-preparation transaction in the caller's address
   space, key points to a record key in the caller's address space (NULL
   returns NULL), and no concurrent operations on funk, txn or key.
   funk retains no interest in key or opt_err.  The funk retains
   ownership of txn and any returned record.  The record value metadata
   will be updated whenever the record value modified.

   This is a reasonably fast O(1) and fortified against memory
   corruption. */

fd_funk_rec_t const *
fd_funk_rec_insert( fd_funk_t *               funk,
                    fd_funk_txn_t *           txn,
                    fd_funk_rec_key_t const * key,
                    int *                     opt_err );

/* fd_funk_rec_remove removes the live record pointed to by rec from
   the funk.  Returns FD_FUNK_SUCCESS (0) on success and a FD_FUNK_ERR_*
   (negative) on failure.  Reasons for failure include:

     FD_FUNK_ERR_INVAL - bad inputs (NULL funk, NULL rec, rec is
       obviously not from funk, etc)

     FD_FUNK_ERR_KEY - the record did not appear to be a live record.
       Specifically, a record query of funk for rec's (xid,key) pair did
       not return rec.

     FD_FUNK_ERR_XID - the record to remove is published but erase was
       not specified.

     FD_FUNK_ERR_FROZEN - rec is part of a transaction that is frozen.

   All changes to the record in that transaction will be undone.

   Further, if erase is zero, if and when the transaction is published
   (assuming no subsequent insert of key into that transaction), no
   changes will be made to the published record.  This type of remove
   cannot be done on a published record.

   However, if erase is non-zero, the record will cease to exist in that
   transaction and any of transaction's subsequently created descendants
   (again, assuming no subsequent insert of key).  This type of remove
   can be done on a published record (assuming the last published
   transaction is unfrozen).

   Any information in an erased record is lost.

   Detailed record erasure handling:

     rec's  | erase | rec's     | rec's | return     | info
     txn    | req   | txn       | erase |            |
     frozen |       | published |       |            |
     -------+-------+-----------+-------+------------+-----
     no     | no    | no        | clear | SUCCESS    | discards updates to a record, rec dead on return
     no     | no    | no        | set   | SUCCESS    | discards erase of most recent ancestor, rec dead on return
     no     | no    | yes       | clear | ERR_XID    | can't revert published record to an older version, rec live on return
     no     | no    | yes       | set   | *LOG_CRIT* | detected corruption, repurpose to allow for unerasable recs?
     no     | yes   | no        | clear | SUCCESS    | erase the most recent ancestor version of rec, if no such ancestor version,
            |       |           |       |            | rec dead on return, otherwise, rec live on return and rec's erase will be
            |       |           |       |            | set, O(ancestor_hops_to_last_publish) worst case
     no     | yes   | no        | set   | SUCCESS    | no-op, previously marked erase, rec live on return
     no     | yes   | yes       | clear | SUCCESS    | erases published record, rec dead on return
     no     | yes   | yes       | set   | *LOG_CRIT* | detected corruption, repurpose to allow for unerasable recs?
     yes    | -     | -         | -     | ERR_FROZEN | can't remove rec because rec's txn is frozen, rec live on return

   On ERR_INVAL and ERR_KEY, rec didn't seem to point to a live record
   on entry with and still doesn't on return.

   Assumes funk is a current local join (NULL returns ERR_INVAL) and rec
   points to a record in the caller's address space (NULL returns
   ERR_INVAL).  As the funk still has ownership of rec before and after
   the call if live, the user doesn't need to, for example, match
   inserts with removes.

   This is a reasonably fast O(1) except in the case noted above and
   fortified against memory corruption.

   IMPORTANT SAFETY TIP!  DO NOT CAST AWAY CONST FROM A FD_FUNK_REC_T TO
   USE THIS FUNCTION (E.G. PASS A RESULT DIRECTLY FROM QUERY).  USE A
   LIVE RESULT FROM FD_FUNK_REC_MODIFY! */

int
fd_funk_rec_remove( fd_funk_t *     funk,
                    fd_funk_rec_t * rec,
                    int             erase );

/* Misc */

/* fd_funk_rec_verify verifies the record map.  Returns FD_FUNK_SUCCESS
   if the record map appears intact and FD_FUNK_ERR_INVAL if not (logs
   details).  Meant to be called as part of fd_funk_verify.  As such, it
   assumes funk is non-NULL, fd_funk_{wksp,txn_map,rec_map} have been
   verified to work and the txn_map has been verified. */

int
fd_funk_rec_verify( fd_funk_t * funk );

FD_PROTOTYPES_END

/* TODO: Consider instead doing something like: modify_init, modify_fini and
   preventing forking the txn if records are being modified instead of
   the long laundry list of lifetime constraints? */

/* TODO: Consider using index compression here (much more debatable than
   in txn itself) */

/* TODO: Consider using self index in the structures to accelerate index
   computations if padding permits and structures aren't powers of 2 in
   size. */

#endif /* HEADER_fd_src_funk_fd_funk_rec_h */
