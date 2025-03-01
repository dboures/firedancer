#include "fd_funk.h"

#if FD_HAS_HOSTED && FD_HAS_X86

/* Provide the actual record map implementation */

#define MAP_NAME              fd_funk_rec_map
#define MAP_T                 fd_funk_rec_t
#define MAP_KEY_T             fd_funk_xid_key_pair_t
#define MAP_KEY               pair
#define MAP_KEY_EQ(k0,k1)     fd_funk_xid_key_pair_eq((k0),(k1))
#define MAP_KEY_HASH(k0,seed) fd_funk_xid_key_pair_hash((k0),(seed))
#define MAP_KEY_COPY(kd,ks)   fd_funk_xid_key_pair_copy((kd),(ks))
#define MAP_NEXT              map_next
#define MAP_MAGIC             (0xf173da2ce77ecdb0UL) /* Firedancer rec db version 0 */
#define MAP_IMPL_STYLE        2
#include "../util/tmpl/fd_map_giant.c"

fd_funk_rec_t const *
fd_funk_rec_query( fd_funk_t *               funk,
                   fd_funk_txn_t const *     txn,
                   fd_funk_rec_key_t const * key ) {

  if( FD_UNLIKELY( (!funk) | (!key) ) ) return NULL;

  fd_funk_xid_key_pair_t pair[1]; fd_funk_xid_key_pair_init( pair, txn ? fd_funk_txn_xid( txn ) : fd_funk_root( funk ), key );

  return fd_funk_rec_map_query( fd_funk_rec_map( funk, fd_funk_wksp( funk ) ), pair, NULL );
}

fd_funk_rec_t const *
fd_funk_rec_query_const( fd_funk_t *               funk,
                         fd_funk_txn_t const *     txn,
                         fd_funk_rec_key_t const * key ) {
  if( FD_UNLIKELY( (!funk) | (!key) ) ) return NULL;

  fd_funk_xid_key_pair_t pair[1]; fd_funk_xid_key_pair_init( pair, txn ? fd_funk_txn_xid( txn ) : fd_funk_root( funk ), key );

  return fd_funk_rec_map_query_const( fd_funk_rec_map( funk, fd_funk_wksp( funk ) ), pair, NULL );
}

fd_funk_rec_t const *
fd_funk_rec_query_global( fd_funk_t *               funk,
                          fd_funk_txn_t const *     txn,
                          fd_funk_rec_key_t const * key ) {

  if( FD_UNLIKELY( (!funk) | (!key) ) ) return NULL;

  fd_wksp_t * wksp = fd_funk_wksp( funk );

  fd_funk_rec_t * rec_map = fd_funk_rec_map( funk, wksp );

  if( txn ) { /* Query txn and its in-preparation ancestors */

    fd_funk_txn_t * txn_map = fd_funk_txn_map( funk, wksp );

    ulong txn_max = funk->txn_max;

    ulong txn_idx = (ulong)(txn - txn_map);

    if( FD_UNLIKELY( (txn_idx>=txn_max) /* Out of map (incl NULL) */ | (txn!=(txn_map+txn_idx)) /* Bad alignment */ ) )
      return NULL;

    /* TODO: const correct and/or fortify? */
    do {
      fd_funk_xid_key_pair_t pair[1]; fd_funk_xid_key_pair_init( pair, fd_funk_txn_xid( txn ), key );
      fd_funk_rec_t const * rec = fd_funk_rec_map_query( rec_map, pair, NULL );
      if( FD_LIKELY( rec ) ) return rec;
      txn = fd_funk_txn_parent( (fd_funk_txn_t *)txn, txn_map );
    } while( FD_UNLIKELY( txn ) );

  }

  /* Query the last published transaction */

  fd_funk_xid_key_pair_t pair[1]; fd_funk_xid_key_pair_init( pair, fd_funk_root( funk ), key );
  return fd_funk_rec_map_query( rec_map, pair, NULL );
}

fd_funk_rec_t const *
fd_funk_rec_query_global_const( fd_funk_t *               funk,
                                fd_funk_txn_t const *     txn,
                                fd_funk_rec_key_t const * key ) {

  if( FD_UNLIKELY( (!funk) | (!key) ) ) return NULL;

  fd_wksp_t * wksp = fd_funk_wksp( funk );

  fd_funk_rec_t * rec_map = fd_funk_rec_map( funk, wksp );

  if( txn ) { /* Query txn and its in-preparation ancestors */

    fd_funk_txn_t * txn_map = fd_funk_txn_map( funk, wksp );

    ulong txn_max = funk->txn_max;

    ulong txn_idx = (ulong)(txn - txn_map);

    if( FD_UNLIKELY( (txn_idx>=txn_max) /* Out of map (incl NULL) */ | (txn!=(txn_map+txn_idx)) /* Bad alignment */ ) )
      return NULL;

    /* TODO: const correct and/or fortify? */
    do {
      fd_funk_xid_key_pair_t pair[1]; fd_funk_xid_key_pair_init( pair, fd_funk_txn_xid( txn ), key );
      fd_funk_rec_t const * rec = fd_funk_rec_map_query_const( rec_map, pair, NULL );
      if( FD_LIKELY( rec ) ) return rec;
      txn = fd_funk_txn_parent( (fd_funk_txn_t *)txn, txn_map );
    } while( FD_UNLIKELY( txn ) );

  }

  /* Query the last published transaction */

  fd_funk_xid_key_pair_t pair[1]; fd_funk_xid_key_pair_init( pair, fd_funk_root( funk ), key );
  return fd_funk_rec_map_query_const( rec_map, pair, NULL );
}

int
fd_funk_rec_test( fd_funk_t *           funk,
                  fd_funk_rec_t const * rec ) {

  if( FD_UNLIKELY( !funk ) ) return FD_FUNK_ERR_INVAL;

  fd_wksp_t * wksp = fd_funk_wksp( funk );

  fd_funk_rec_t * rec_map = fd_funk_rec_map( funk, wksp );

  ulong rec_max = funk->rec_max;

  ulong rec_idx = (ulong)(rec - rec_map);

  if( FD_UNLIKELY( (rec_idx>=rec_max) /* Out of map (incl NULL) */ | (rec!=(rec_map+rec_idx)) /* Bad alignment */ ) )
    return FD_FUNK_ERR_INVAL;

  if( FD_UNLIKELY( rec!=fd_funk_rec_map_query_const( rec_map, fd_funk_rec_pair( rec ), NULL ) ) ) return FD_FUNK_ERR_KEY;

  ulong txn_idx = fd_funk_txn_idx( rec->txn_cidx );

  if( FD_UNLIKELY( fd_funk_txn_idx_is_null( txn_idx ) ) ) { /* Rec in last published, opt for lots recs */

    if( FD_UNLIKELY( fd_funk_last_publish_is_frozen( funk ) ) ) return FD_FUNK_ERR_FROZEN;

  } else { /* Rec in in-preparation */

    fd_funk_txn_t * txn_map = fd_funk_txn_map( funk, wksp );
    ulong           txn_max = funk->txn_max;

    if( FD_UNLIKELY( txn_idx>=txn_max ) ) return FD_FUNK_ERR_XID; /* TODO: consider LOG_CRIT here? */

    if( FD_UNLIKELY( fd_funk_txn_is_frozen( &txn_map[ txn_idx ] ) ) ) return FD_FUNK_ERR_FROZEN;

  }

  return FD_FUNK_SUCCESS;
}

fd_funk_rec_t *
fd_funk_rec_modify( fd_funk_t *           funk,
                    fd_funk_rec_t const * rec ) {

  if( FD_UNLIKELY( (!funk) | (!rec) ) ) return NULL;

  fd_wksp_t * wksp = fd_funk_wksp( funk );

  fd_funk_rec_t * rec_map = fd_funk_rec_map( funk, wksp );

  ulong rec_max = funk->rec_max;

  ulong rec_idx = (ulong)(rec - rec_map);

  if( FD_UNLIKELY( (rec_idx>=rec_max) /* Out of map (incl NULL) */ | (rec!=(rec_map+rec_idx)) /* Bad alignment */ ) )
    return NULL;

  if( FD_UNLIKELY( rec!=fd_funk_rec_map_query( rec_map, fd_funk_rec_pair( rec ), NULL ) ) ) return NULL; /* Not live */

  ulong txn_idx = fd_funk_txn_idx( rec->txn_cidx );

  if( fd_funk_txn_idx_is_null( txn_idx ) ) { /* Modifying last published transaction */

    if( FD_UNLIKELY( fd_funk_last_publish_is_frozen( funk ) ) ) return NULL;

  } else { /* Modifying an in-prep tranaction */

    fd_funk_txn_t * txn_map = fd_funk_txn_map( funk, wksp );

    ulong txn_max = funk->txn_max;

    if( FD_UNLIKELY( txn_idx>=txn_max ) ) FD_LOG_CRIT(( "memory corruption detected (bad idx)" ));

    if( FD_UNLIKELY( fd_funk_txn_is_frozen( &txn_map[ txn_idx ] ) ) ) return NULL;
  }

  return (fd_funk_rec_t *)rec;
}

fd_funk_rec_t const *
fd_funk_rec_insert( fd_funk_t *               funk,
                    fd_funk_txn_t *           txn,
                    fd_funk_rec_key_t const * key,
                    int *                     opt_err ) {

  if( FD_UNLIKELY( (!funk) |     /* NULL funk */
                   (!key ) ) ) { /* NULL key */
    fd_int_store_if( !!opt_err, opt_err, FD_FUNK_ERR_INVAL );
    return NULL;
  }

  fd_wksp_t * wksp = fd_funk_wksp( funk );

  fd_funk_rec_t * rec_map = fd_funk_rec_map( funk, wksp );

  ulong rec_max = funk->rec_max;

  if( FD_UNLIKELY( fd_funk_rec_map_is_full( rec_map ) ) ) {
    fd_int_store_if( !!opt_err, opt_err, FD_FUNK_ERR_REC );
    return NULL;
  }

  ulong                  txn_idx;
  ulong *                _rec_head_idx;
  ulong *                _rec_tail_idx;
  fd_funk_xid_key_pair_t pair[1];

  if( !txn ) { /* Modifying last published */

    if( FD_UNLIKELY( fd_funk_last_publish_is_frozen( funk ) ) ) {
      fd_int_store_if( !!opt_err, opt_err, FD_FUNK_ERR_FROZEN );
      return NULL;
    }

    txn_idx       = FD_FUNK_TXN_IDX_NULL;
    _rec_head_idx = &funk->rec_head_idx;
    _rec_tail_idx = &funk->rec_tail_idx;

    fd_funk_xid_key_pair_init( pair, fd_funk_root( funk ), key );

    fd_funk_rec_t * rec = fd_funk_rec_map_query( rec_map, pair, NULL );

    if( FD_UNLIKELY( rec ) ) { /* Already a record present */
      if( FD_UNLIKELY( rec->flags & FD_FUNK_REC_FLAG_ERASE ) ) FD_LOG_CRIT(( "memory corruption detected (bad flags)" ));
      fd_int_store_if( !!opt_err, opt_err, FD_FUNK_ERR_KEY );
      return NULL;
    }

  } else { /* Modifying in-preparation */

    fd_funk_txn_t * txn_map = fd_funk_txn_map( funk, wksp );
  
    ulong txn_max = funk->txn_max;
  
    txn_idx       = (ulong)(txn - txn_map);
    _rec_head_idx = &txn->rec_head_idx;
    _rec_tail_idx = &txn->rec_tail_idx;

    if( FD_UNLIKELY( (txn_idx>=txn_max) /* Out of map (incl NULL) */ | (txn!=(txn_map+txn_idx)) /* Bad alignment */ ) ) {
      fd_int_store_if( !!opt_err, opt_err, FD_FUNK_ERR_INVAL );
      return NULL;
    }

    if( FD_UNLIKELY( !fd_funk_txn_map_query( txn_map, fd_funk_txn_xid( txn ), NULL ) ) ) {
      fd_int_store_if( !!opt_err, opt_err, FD_FUNK_ERR_INVAL );
      return NULL;
    }

    if( FD_UNLIKELY( fd_funk_txn_is_frozen( txn ) ) ) {
      fd_int_store_if( !!opt_err, opt_err, FD_FUNK_ERR_FROZEN );
      return NULL;
    }

    fd_funk_xid_key_pair_init( pair, fd_funk_txn_xid( txn ), key );

    fd_funk_rec_t * rec = fd_funk_rec_map_query( rec_map, pair, NULL );

    if( FD_UNLIKELY( rec ) ) { /* Already a record present */
      if( FD_UNLIKELY( rec->flags & FD_FUNK_REC_FLAG_ERASE ) ) {
        rec->flags &= ~FD_FUNK_REC_FLAG_ERASE; /* Undo any previous erase (note the val was already removed on erase) */
        return rec;
      }
      fd_int_store_if( !!opt_err, opt_err, FD_FUNK_ERR_KEY );
      return NULL;
    }

  }

  fd_funk_rec_t * rec     = fd_funk_rec_map_insert( rec_map, pair );
  ulong           rec_idx = (ulong)(rec - rec_map);
  if( FD_UNLIKELY( rec_idx>=rec_max ) ) FD_LOG_CRIT(( "memory corruption detected (bad idx)" ));

  ulong rec_prev_idx = *_rec_tail_idx;

  int first_born = fd_funk_rec_idx_is_null( rec_prev_idx );
  if( FD_UNLIKELY( !first_born ) ) {
    if( FD_UNLIKELY( rec_prev_idx>=rec_max ) )
      FD_LOG_CRIT(( "memory corruption detected (bad_idx)" ));
    if( FD_UNLIKELY( fd_funk_txn_idx( rec_map[ rec_prev_idx ].txn_cidx!=txn_idx ) ) )
      FD_LOG_CRIT(( "memory corruption detected (mismatch)" ));
  }

  rec->prev_idx = rec_prev_idx;
  rec->next_idx = FD_FUNK_REC_IDX_NULL;
  rec->txn_cidx = fd_funk_txn_cidx( txn_idx );
  rec->tag      = 0U;
  rec->flags    = 0UL;

  if( first_born ) *_rec_head_idx                   = rec_idx;
  else             rec_map[ rec_prev_idx ].next_idx = rec_idx;

  *_rec_tail_idx = rec_idx;

  /* Val management stuff here */

  fd_int_store_if( !!opt_err, opt_err, FD_FUNK_SUCCESS );
  return rec;
}

int
fd_funk_rec_remove( fd_funk_t *     funk,
                    fd_funk_rec_t * rec,
                    int             erase ) {

  if( FD_UNLIKELY( !funk ) ) return FD_FUNK_ERR_INVAL;

  fd_wksp_t * wksp = fd_funk_wksp( funk );

  fd_funk_rec_t * rec_map = fd_funk_rec_map( funk, wksp );

  ulong rec_max = funk->rec_max;

  ulong rec_idx = (ulong)(rec - rec_map);

  if( FD_UNLIKELY( (rec_idx>=rec_max) /* Out of map (incl NULL) */ | (rec!=(rec_map+rec_idx)) /* Bad alignment */ ) )
    return FD_FUNK_ERR_INVAL;

  if( FD_UNLIKELY( rec!=fd_funk_rec_map_query_const( rec_map, fd_funk_rec_pair( rec ), NULL ) ) ) return FD_FUNK_ERR_KEY;

  /* At this point, rec appears to be a live record.  Determine which
     list contains the record and if we are allowed to remove it. */

  ulong * _rec_head_idx;
  ulong * _rec_tail_idx;

  ulong txn_idx = fd_funk_txn_idx( rec->txn_cidx );

  if( FD_UNLIKELY( fd_funk_txn_idx_is_null( txn_idx ) ) ) { /* Removing from last published, opt for lots recs, rand remove */

    if( FD_UNLIKELY( fd_funk_last_publish_is_frozen( funk ) ) ) return FD_FUNK_ERR_FROZEN;

    if( FD_UNLIKELY( rec->flags & FD_FUNK_REC_FLAG_ERASE ) ) FD_LOG_CRIT(( "memory corruption detected (bad flags)" ));

    if( FD_UNLIKELY( !erase ) ) return FD_FUNK_ERR_XID;

    /* At this point, we are in last published transaction, it is
       unfrozen, the record erase flag is clear and the user explicitly
       asked to erase.  Remove the record. */

    _rec_head_idx = &funk->rec_head_idx;
    _rec_tail_idx = &funk->rec_tail_idx;

  } else { /* Removing from in-preparation transaction */

    fd_funk_txn_t * txn_map = fd_funk_txn_map( funk, wksp );
    ulong           txn_max = funk->txn_max;

    if( FD_UNLIKELY( txn_idx>=txn_max ) ) FD_LOG_CRIT(( "memory corruption detected (bad idx)" ));

    if( FD_UNLIKELY( fd_funk_txn_is_frozen( &txn_map[ txn_idx ] ) ) ) return FD_FUNK_ERR_FROZEN;

    if( FD_UNLIKELY( erase ) ) {
      if( FD_UNLIKELY( rec->flags & FD_FUNK_REC_FLAG_ERASE ) ) return FD_FUNK_SUCCESS; /* Already marked for erase */

      /* Release value resources here */

      /* Query our ancestors to see if we need to keep this record
         around or if we can just remove it immediately.  Though this is
         potentially an O(ancestor_cnt) cost, it prevents the
         possibility unnecessarily consuming a practically unbounded
         number of records by flickering insert / remove-with-erase in
         an in-preparaton transaction with lots unique keys. */

      ulong tag = ((ulong)fd_tickcount()) << 2; /* TODO: Use fd_funk_txn_cycle_tag from fd_funk_txn.c */

      ulong cur_idx = txn_idx;
      for(;;) {

        /* At this point, transaction cur_idx is an in-progress
           transaction.  Tag it for cycle detection and see if
           transaction cur_idx's parent has a record for this and react
           accordingly. */

        txn_map[ cur_idx ].tag = tag;

        ulong parent_idx = fd_funk_txn_idx( txn_map[ cur_idx ].parent_cidx );
        if( FD_LIKELY( fd_funk_txn_idx_is_null( parent_idx ) ) ) { /* Parent txn is last published, opt for shallow */

          fd_funk_rec_t const * erase_rec = fd_funk_rec_query( funk, NULL, fd_funk_rec_key( rec ) );
          if( FD_UNLIKELY( !erase_rec ) ) break; /* No ancestor has this record, can free immediately, opt no flicker */

          /* Record is available in last published ... this remove
             erases that record on publish of this txn */
          if( FD_UNLIKELY( erase_rec->flags & FD_FUNK_REC_FLAG_ERASE ) ) FD_LOG_CRIT(( "memory corruption detected (bad flags)" ));
          rec->flags |= FD_FUNK_REC_FLAG_ERASE;
          return FD_FUNK_SUCCESS;

        }

        if( FD_UNLIKELY( parent_idx>=txn_max )            ) FD_LOG_CRIT(( "memory corruption detected (bad idx)" ));
        if( FD_UNLIKELY( txn_map[ parent_idx ].tag==tag ) ) FD_LOG_CRIT(( "memory corruption detected (cycle)" ));

        fd_funk_rec_t const * erase_rec = fd_funk_rec_query( funk, &txn_map[ parent_idx ], fd_funk_rec_key( rec ) );
        if( FD_LIKELY( erase_rec ) ) { /* Opt for shallow */
          /* Record is available in in-progress ancestor ... this remove
             erases that record on publish of this txn */
          if( erase_rec->flags & FD_FUNK_REC_FLAG_ERASE ) break; /* Already marked for erase, can free this record */
          rec->flags |= FD_FUNK_REC_FLAG_ERASE;
          return FD_FUNK_SUCCESS;
        }

        cur_idx = parent_idx;
      }

    }

    /* At this point, we are in an in-prep transaction, it is unfrozen,
       and we are to discard changes to this record done by this
       transaction.  Note that if rec_erase is set, this will discard
       the erase and any value changes that might have been made
       previously. */

    _rec_head_idx = &txn_map[ txn_idx ].rec_head_idx;
    _rec_tail_idx = &txn_map[ txn_idx ].rec_tail_idx;
  }

  /* Remove the record from its list */

  ulong prev_idx = rec->prev_idx;
  ulong next_idx = rec->next_idx;

  int prev_null = fd_funk_rec_idx_is_null( prev_idx );
  int next_null = fd_funk_rec_idx_is_null( next_idx );

  if( !( ((prev_null) | (prev_idx<rec_max)) & ((next_null) | (next_idx<rec_max)) ) )
    FD_LOG_CRIT(( "memory corruption detected (bad idx)" ));

  /* TODO: Consider branchless impl */
  if( prev_null ) *_rec_head_idx               = next_idx;
  else            rec_map[ prev_idx ].next_idx = next_idx;

  if( next_null ) *_rec_tail_idx               = prev_idx;
  else            rec_map[ next_idx ].prev_idx = prev_idx;

  /* Val management stuff here */

  fd_funk_rec_map_remove( rec_map, fd_funk_rec_pair( rec ) );
  return FD_FUNK_SUCCESS;
}

int
fd_funk_rec_verify( fd_funk_t * funk ) {
  fd_wksp_t *     wksp    = fd_funk_wksp( funk );          /* Previously verified */
  fd_funk_txn_t * txn_map = fd_funk_txn_map( funk, wksp ); /* Previously verified */
  fd_funk_rec_t * rec_map = fd_funk_rec_map( funk, wksp ); /* Previously verified */
  ulong           txn_max = funk->txn_max;                 /* Previously verified */
  ulong           rec_max = funk->rec_max;                 /* Previously verified */

  /* At this point, txn_map has been extensively verified */

# define TEST(c) do {                                                                           \
    if( FD_UNLIKELY( !(c) ) ) { FD_LOG_WARNING(( "FAIL: %s", #c )); return FD_FUNK_ERR_INVAL; } \
  } while(0)

  TEST( !fd_funk_rec_map_verify( rec_map ) );

  /* Iterate over all records in use */

  for( fd_funk_rec_map_iter_t iter = fd_funk_rec_map_iter_init( rec_map );
       !fd_funk_rec_map_iter_done( rec_map, iter );
       iter = fd_funk_rec_map_iter_next( rec_map, iter ) ) {
    fd_funk_rec_t * rec = fd_funk_rec_map_iter_ele( rec_map, iter );

    /* Make sure every record either links up with the last published
       transaction or an in-preparation transaction and the flags are
       sane. */

    fd_funk_txn_xid_t const * txn_xid = fd_funk_rec_xid( rec );
    ulong                     txn_idx = fd_funk_txn_idx( rec->txn_cidx );

    if( fd_funk_txn_idx_is_null( txn_idx ) ) { /* This is a record from the last published transaction */

      TEST( fd_funk_txn_xid_eq_root( txn_xid ) );
      TEST( !(rec->flags & FD_FUNK_REC_FLAG_ERASE) );

    } else { /* This is a record from an in-progress transaction */

      TEST( txn_idx<txn_max );
      fd_funk_txn_t const * txn = fd_funk_txn_map_query_const( txn_map, txn_xid, NULL );
      TEST( txn );
      TEST( txn==(txn_map+txn_idx) );

      /* At this point, txn points to a sane value and map verify has already
         ensured txn_map has no cycles.  So we are safe to call
         query_global_const here.  TODO: const correct. */

      if( (rec->flags & FD_FUNK_REC_FLAG_ERASE) ) {
        fd_funk_rec_t const * erase_rec =
          fd_funk_rec_query_global_const( funk, fd_funk_txn_parent( (fd_funk_txn_t *)txn, txn_map ), fd_funk_rec_key( rec ) );
        TEST( erase_rec && !(erase_rec->flags & FD_FUNK_REC_FLAG_ERASE) );
      }

    }
  }

  /* Clear record tags and then verify the forward and reverse linkage */

  for( ulong rec_idx=0UL; rec_idx<rec_max; rec_idx++ ) rec_map[ rec_idx ].tag = 0U;

  ulong rec_cnt = fd_funk_rec_map_key_cnt( rec_map );

  do {
    ulong cnt = 0UL;

    ulong txn_idx = FD_FUNK_TXN_IDX_NULL;
    ulong rec_idx = funk->rec_head_idx;
    while( !fd_funk_rec_idx_is_null( rec_idx ) ) {
      TEST( (rec_idx<rec_max) && (fd_funk_txn_idx( rec_map[ rec_idx ].txn_cidx )==txn_idx) && rec_map[ rec_idx ].tag==0U );
      rec_map[ rec_idx ].tag = 1U;
      cnt++;
      ulong next_idx = rec_map[ rec_idx ].next_idx;
      if( !fd_funk_rec_idx_is_null( next_idx ) ) TEST( rec_map[ next_idx ].prev_idx==rec_idx );
      rec_idx = next_idx;
    }
    for( fd_funk_txn_map_iter_t iter = fd_funk_txn_map_iter_init( txn_map );
         !fd_funk_txn_map_iter_done( txn_map, iter );
         iter = fd_funk_txn_map_iter_next( txn_map, iter ) ) {
      fd_funk_txn_t * txn = fd_funk_txn_map_iter_ele( txn_map, iter );

      ulong txn_idx = (ulong)(txn-txn_map);
      ulong rec_idx = txn->rec_head_idx;
      while( !fd_funk_rec_idx_is_null( rec_idx ) ) {
        TEST( (rec_idx<rec_max) && (fd_funk_txn_idx( rec_map[ rec_idx ].txn_cidx )==txn_idx) && rec_map[ rec_idx ].tag==0U );
        rec_map[ rec_idx ].tag = 1U;
        cnt++;
        ulong next_idx = rec_map[ rec_idx ].next_idx;
        if( !fd_funk_rec_idx_is_null( next_idx ) ) TEST( rec_map[ next_idx ].prev_idx==rec_idx );
        rec_idx = next_idx;
      }
    }

    TEST( cnt==rec_cnt );
  } while(0);

  do {
    ulong cnt = 0UL;

    ulong txn_idx = FD_FUNK_TXN_IDX_NULL;
    ulong rec_idx = funk->rec_tail_idx;
    while( !fd_funk_rec_idx_is_null( rec_idx ) ) {
      TEST( (rec_idx<rec_max) && (fd_funk_txn_idx( rec_map[ rec_idx ].txn_cidx )==txn_idx) && rec_map[ rec_idx ].tag==1U );
      rec_map[ rec_idx ].tag = 2U;
      cnt++;
      ulong prev_idx = rec_map[ rec_idx ].prev_idx;
      if( !fd_funk_rec_idx_is_null( prev_idx ) ) TEST( rec_map[ prev_idx ].next_idx==rec_idx );
      rec_idx = prev_idx;
    }

    for( fd_funk_txn_map_iter_t iter = fd_funk_txn_map_iter_init( txn_map );
         !fd_funk_txn_map_iter_done( txn_map, iter );
         iter = fd_funk_txn_map_iter_next( txn_map, iter ) ) {
      fd_funk_txn_t * txn = fd_funk_txn_map_iter_ele( txn_map, iter );

      ulong txn_idx = (ulong)(txn-txn_map);
      ulong rec_idx = txn->rec_tail_idx;
      while( !fd_funk_rec_idx_is_null( rec_idx ) ) {
        TEST( (rec_idx<rec_max) && (fd_funk_txn_idx( rec_map[ rec_idx ].txn_cidx )==txn_idx) && rec_map[ rec_idx ].tag==1U );
        rec_map[ rec_idx ].tag = 2U;
        cnt++;
        ulong prev_idx = rec_map[ rec_idx ].prev_idx;
        if( !fd_funk_rec_idx_is_null( prev_idx ) ) TEST( rec_map[ prev_idx ].next_idx==rec_idx );
        rec_idx = prev_idx;
      }
    }

    TEST( cnt==rec_cnt );
  } while(0);

# undef TEST

  return FD_FUNK_SUCCESS;
}

#endif /* FD_HAS_HOSTED && FD_HAS_X86 */
