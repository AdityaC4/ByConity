/*
 * Copyright 2016-2023 ClickHouse, Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/*
 * This file may have been modified by Bytedance Ltd. and/or its affiliates (“ Bytedance's Modifications”).
 * All Bytedance's Modifications are Copyright (2023) Bytedance Ltd. and/or its affiliates.
 */

#include <IO/Operators.h>
#include <Parsers/ASTSystemQuery.h>
#include <Parsers/IAST.h>
#include <Common/quoteString.h>


namespace DB
{
namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
}

const char * metaOpsToString(MetastoreOperation opt)
{
    switch (opt)
    {
        case MetastoreOperation::SYNC:
            return "SYNC";
        case MetastoreOperation::START_AUTO_SYNC:
            return "START AUTO SYNC";
        case MetastoreOperation::STOP_AUTO_SYNC:
            return "STOP AUTO SYNC";
        case MetastoreOperation::DROP_ALL_KEY:
            return "DROP";
        case MetastoreOperation::DROP_BY_KEY:
            return "DROP BY KEY";
        default:
            throw Exception("Unknown metastore operation.", ErrorCodes::LOGICAL_ERROR);
    }
}


const char * ASTSystemQuery::typeToString(Type type)
{
    switch (type)
    {
        case Type::SHUTDOWN:
            return "SHUTDOWN";
        case Type::KILL:
            return "KILL";
        case Type::SUSPEND:
            return "SUSPEND";
        case Type::DROP_DNS_CACHE:
            return "DROP DNS CACHE";
        case Type::DROP_MARK_CACHE:
            return "DROP MARK CACHE";
        case Type::DROP_UNCOMPRESSED_CACHE:
            return "DROP UNCOMPRESSED CACHE";
        case Type::DROP_MMAP_CACHE:
            return "DROP MMAP CACHE";
        case Type::DROP_CHECKSUMS_CACHE:
            return "DROP CHECKSUMS CACHE";
        case Type::DROP_CNCH_PART_CACHE:
            return "DROP CNCH PART CACHE";
#if USE_EMBEDDED_COMPILER
        case Type::DROP_COMPILED_EXPRESSION_CACHE:
            return "DROP COMPILED EXPRESSION CACHE";
#endif
        case Type::STOP_LISTEN_QUERIES:
            return "STOP LISTEN QUERIES";
        case Type::START_LISTEN_QUERIES:
            return "START LISTEN QUERIES";
        case Type::RESTART_REPLICAS:
            return "RESTART REPLICAS";
        case Type::RESTART_REPLICA:
            return "RESTART REPLICA";
        case Type::RESTORE_REPLICA:
            return "RESTORE REPLICA";
        case Type::DROP_REPLICA:
            return "DROP REPLICA";
        case Type::SYNC_REPLICA:
            return "SYNC REPLICA";
        case Type::FLUSH_DISTRIBUTED:
            return "FLUSH DISTRIBUTED";
        case Type::START_RESOURCE_GROUP:
            return "START RESOURCE GROUP";
        case Type::STOP_RESOURCE_GROUP:
            return "STOP RESOURCE GROUP";
        case Type::RELOAD_DICTIONARY:
            return "RELOAD DICTIONARY";
        case Type::RELOAD_DICTIONARIES:
            return "RELOAD DICTIONARIES";
        case Type::RELOAD_MODEL:
            return "RELOAD MODEL";
        case Type::RELOAD_MODELS:
            return "RELOAD MODELS";
        case Type::RELOAD_EMBEDDED_DICTIONARIES:
            return "RELOAD EMBEDDED DICTIONARIES";
        case Type::RELOAD_CONFIG:
            return "RELOAD CONFIG";
        case Type::RELOAD_FORMAT_SCHEMA:
            return "RELOAD FORMAT SCHEMA";
        case Type::RELOAD_SYMBOLS:
            return "RELOAD SYMBOLS";
        case Type::STOP_MERGES:
            return "STOP MERGES";
        case Type::START_MERGES:
            return "START MERGES";
        case Type::REMOVE_MERGES:
            return "REMOVE MERGES";
        case Type::START_GC:
            return "START GC";
        case Type::STOP_GC:
            return "STOP GC";
        case Type::FORCE_GC:
            return "FORCE GC";
        case Type::STOP_TTL_MERGES:
            return "STOP TTL MERGES";
        case Type::START_TTL_MERGES:
            return "START TTL MERGES";
        case Type::STOP_MOVES:
            return "STOP MOVES";
        case Type::START_MOVES:
            return "START MOVES";
        case Type::STOP_FETCHES:
            return "STOP FETCHES";
        case Type::START_FETCHES:
            return "START FETCHES";
        case Type::STOP_REPLICATED_SENDS:
            return "STOP REPLICATED SENDS";
        case Type::START_REPLICATED_SENDS:
            return "START REPLICATED SENDS";
        case Type::STOP_REPLICATION_QUEUES:
            return "STOP REPLICATION QUEUES";
        case Type::START_REPLICATION_QUEUES:
            return "START REPLICATION QUEUES";
        case Type::STOP_DISTRIBUTED_SENDS:
            return "STOP DISTRIBUTED SENDS";
        case Type::START_DISTRIBUTED_SENDS:
            return "START DISTRIBUTED SENDS";
        case Type::FLUSH_LOGS:
            return "FLUSH LOGS";
        case Type::FLUSH_CNCH_LOG:
            return "FLUSH CNCH LOG";
        case Type::STOP_CNCH_LOG:
            return "STOP CNCH LOG";
        case Type::RESUME_CNCH_LOG:
            return "RESUME CNCH LOG";
        case Type::RESTART_DISK:
            return "RESTART DISK";
        case Type::START_CONSUME:
            return "START CONSUME";
        case Type::STOP_CONSUME:
            return "STOP CONSUME";
        case Type::RESTART_CONSUME:
            return "RESTART CONSUME";
        case Type::RESET_CONSUME_OFFSET:
            return "RESET CONSUME OFFSET";
        case Type::FETCH_PARTS:
            return "FETCH PARTS INTO";
        case Type::METASTORE:
            return "METASTORE";
        case Type::CLEAR_BROKEN_TABLES:
            return "CLEAR BROKEN TABLES";
        case Type::DEDUP:
            return "DEDUP";
        case Type::SYNC_DEDUP_WORKER:
            return "SYNC DEDUP WORKER";
        case Type::START_DEDUP_WORKER:
            return "START DEDUP WORKER";
        case Type::STOP_DEDUP_WORKER:
            return "STOP DEDUP WORKER";
        case Type::DUMP_SERVER_STATUS:
            return "DUMP SERVER STATUS";
        case Type::CLEAN_TRANSACTION:
            return "CLEAN TRANSACTION";
        case Type::CLEAN_FILESYSTEM_LOCK:
            return "CLEAN FILESYSTEM LOCK";
        case Type::JEPROF_DUMP:
            return "JEPROF DUMP";
        case Type::LOCK_MEMORY_LOCK:
            return "LOCK MEMORY LOCK";
        case Type::UNKNOWN:
        case Type::END:
            throw Exception(ErrorCodes::LOGICAL_ERROR, "Unknown SYSTEM query command");
    }
    __builtin_unreachable();
}


void ASTSystemQuery::formatImpl(const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const
{
    settings.ostr << (settings.hilite ? hilite_keyword : "") << "SYSTEM ";
    settings.ostr << typeToString(type) << (settings.hilite ? hilite_none : "");

    auto print_database_table = [&] {
        settings.ostr << " ";
        if (!database.empty())
        {
            settings.ostr << (settings.hilite ? hilite_identifier : "") << backQuoteIfNeed(database) << (settings.hilite ? hilite_none : "")
                          << ".";
        }
        settings.ostr << (settings.hilite ? hilite_identifier : "") << backQuoteIfNeed(table) << (settings.hilite ? hilite_none : "");
    };

    auto print_drop_replica = [&] {
        settings.ostr << " " << quoteString(replica);
        if (!table.empty())
        {
            settings.ostr << (settings.hilite ? hilite_keyword : "") << " FROM TABLE" << (settings.hilite ? hilite_none : "");
            print_database_table();
        }
        else if (!replica_zk_path.empty())
        {
            settings.ostr << (settings.hilite ? hilite_keyword : "") << " FROM ZKPATH " << (settings.hilite ? hilite_none : "")
                          << quoteString(replica_zk_path);
        }
        else if (!database.empty())
        {
            settings.ostr << (settings.hilite ? hilite_keyword : "") << " FROM DATABASE " << (settings.hilite ? hilite_none : "");
            settings.ostr << (settings.hilite ? hilite_identifier : "") << backQuoteIfNeed(database)
                          << (settings.hilite ? hilite_none : "");
        }
    };

    auto print_on_volume = [&] {
        settings.ostr << (settings.hilite ? hilite_keyword : "") << " ON VOLUME " << (settings.hilite ? hilite_identifier : "")
                      << backQuoteIfNeed(storage_policy) << (settings.hilite ? hilite_none : "") << "."
                      << (settings.hilite ? hilite_identifier : "") << backQuoteIfNeed(volume) << (settings.hilite ? hilite_none : "");
    };

    auto print_metastore_options = [&] {
        settings.ostr << " " << (settings.hilite ? hilite_keyword : "") << metaOpsToString(meta_ops.operation);
        if (!meta_ops.drop_key.empty())
        {
            settings.ostr << " " << quoteString(meta_ops.drop_key);
        }
    };

    if (!cluster.empty())
        formatOnCluster(settings);

    if (type == Type::STOP_MERGES || type == Type::START_MERGES || type == Type::STOP_TTL_MERGES || type == Type::START_TTL_MERGES
        || type == Type::STOP_MOVES || type == Type::START_MOVES || type == Type::STOP_FETCHES || type == Type::START_FETCHES
        || type == Type::STOP_REPLICATED_SENDS || type == Type::START_REPLICATED_SENDS || type == Type::STOP_REPLICATION_QUEUES
        || type == Type::START_REPLICATION_QUEUES || type == Type::STOP_DISTRIBUTED_SENDS || type == Type::START_DISTRIBUTED_SENDS
        || type == Type::DROP_CHECKSUMS_CACHE)
    {
        if (!table.empty())
            print_database_table();
        else if (!volume.empty())
            print_on_volume();
    }
    else if (
        type == Type::RESTART_REPLICA || type == Type::RESTORE_REPLICA || type == Type::SYNC_REPLICA || type == Type::FLUSH_DISTRIBUTED
        || type == Type::RELOAD_DICTIONARY || type == Type::START_CONSUME || type == Type::STOP_CONSUME || type == Type::RESTART_CONSUME
        || type == Type::DROP_CNCH_PART_CACHE || type == Type::SYNC_DEDUP_WORKER || type == Type::DROP_CNCH_PART_CACHE)
    {
        print_database_table();
    }
    else if (type == Type::DROP_REPLICA)
    {
        print_drop_replica();
    }
    else if (type == Type::SUSPEND)
    {
        settings.ostr << (settings.hilite ? hilite_keyword : "") << " FOR " << (settings.hilite ? hilite_none : "") << seconds
                      << (settings.hilite ? hilite_keyword : "") << " SECOND" << (settings.hilite ? hilite_none : "");
    }
    else if (type == Type::FETCH_PARTS)
    {
        print_database_table();
        settings.ostr << " ";
        target_path->formatImpl(settings, state, frame);
    }
    else if (type == Type::METASTORE)
    {
        print_metastore_options();
        if (meta_ops.operation > MetastoreOperation::STOP_AUTO_SYNC)
            print_database_table();
    }
    else if (type == Type::DEDUP)
    {
        print_database_table();
        if (partition)
        {
            settings.ostr << (settings.hilite ? hilite_keyword : "") << " PARTITION " << (settings.hilite ? hilite_none : "");
            partition->formatImpl(settings, state, frame);
        }
        settings.ostr << " FOR REPAIR";
    }
    else if (type == Type::CLEAN_TRANSACTION)
    {
        settings.ostr << " " << txn_id;
    }
}

ASTPtr ASTSystemQuery::clone() const
{
    auto res = std::make_shared<ASTSystemQuery>(*this);
    res->children.clear();

    if (predicate)
    {
        res->predicate = predicate->clone();
        res->children.push_back(res->predicate);
    }
    if (values_changes)
    {
        res->values_changes = values_changes->clone();
        res->children.push_back(res->values_changes);
    }

    return res;
}


}
