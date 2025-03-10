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

#include <Common/typeid_cast.h>
#include <Parsers/ASTFunction.h>
#include <Parsers/ASTIdentifier.h>
#include <Parsers/ASTIndexDeclaration.h>
#include <Parsers/ASTProjectionDeclaration.h>
#include <Parsers/ASTExpressionList.h>
#include <Parsers/ASTCreateQuery.h>
#include <Parsers/ASTSetQuery.h>
#include <Parsers/ASTSelectWithUnionQuery.h>
#include <Parsers/ExpressionListParsers.h>
#include <Parsers/ParserCreateQuery.h>
#include <Parsers/ParserSelectWithUnionQuery.h>
#include <Parsers/ParserSetQuery.h>
#include <Parsers/ASTConstraintDeclaration.h>
#include <Parsers/ParserDictionary.h>
#include <Parsers/ParserDictionaryAttributeDeclaration.h>
#include <Parsers/ParserProjectionSelectQuery.h>
#include <IO/ReadHelpers.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int BAD_ARGUMENTS;
}

bool ParserNestedTable::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserToken open(TokenType::OpeningRoundBracket);
    ParserToken close(TokenType::ClosingRoundBracket);
    ParserIdentifier name_p;
    ParserNameTypePairList columns_p;

    ASTPtr name;
    ASTPtr columns;

    /// For now `name == 'Nested'`, probably alternative nested data structures will appear
    if (!name_p.parse(pos, name, expected))
        return false;

    if (!open.ignore(pos))
        return false;

    if (!columns_p.parse(pos, columns, expected))
        return false;

    if (!close.ignore(pos))
        return false;

    auto func = std::make_shared<ASTFunction>();
    tryGetIdentifierNameInto(name, func->name);
    // FIXME(ilezhankin): func->no_empty_args = true; ?
    func->arguments = columns;
    func->children.push_back(columns);
    node = func;

    return true;
}


bool ParserIdentifierWithParameters::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    return ParserFunction(dt).parse(pos, node, expected);
}

bool ParserNameTypePairList::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    return ParserList(std::make_unique<ParserNameTypePair>(), std::make_unique<ParserToken>(TokenType::Comma), false)
        .parse(pos, node, expected);
}

bool ParserColumnDeclarationList::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    return ParserList(std::make_unique<ParserColumnDeclaration>(dt), std::make_unique<ParserToken>(TokenType::Comma), false)
        .parse(pos, node, expected);
}

bool ParserNameList::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    return ParserList(std::make_unique<ParserCompoundIdentifier>(), std::make_unique<ParserToken>(TokenType::Comma), false)
        .parse(pos, node, expected);
}

bool ParserIndexDeclaration::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_type("TYPE");
    ParserKeyword s_granularity("GRANULARITY");

    ParserIdentifier name_p;
    ParserDataType data_type_p;
    ParserExpression expression_p(dt);
    ParserUnsignedInteger granularity_p;

    ASTPtr name;
    ASTPtr expr;
    ASTPtr type;
    ASTPtr granularity;

    if (!name_p.parse(pos, name, expected))
        return false;

    if (!expression_p.parse(pos, expr, expected))
        return false;

    if (!s_type.ignore(pos, expected))
        return false;

    if (!data_type_p.parse(pos, type, expected))
        return false;

    if (!s_granularity.ignore(pos, expected))
        return false;

    if (!granularity_p.parse(pos, granularity, expected))
        return false;

    auto index = std::make_shared<ASTIndexDeclaration>();
    index->name = name->as<ASTIdentifier &>().name();
    index->granularity = granularity->as<ASTLiteral &>().value.safeGet<UInt64>();
    index->set(index->expr, expr);
    index->set(index->type, type);
    node = index;

    return true;
}

bool ParserConstraintDeclaration::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_check("CHECK");

    ParserIdentifier name_p;
    ParserLogicalOrExpression expression_p(dt);

    ASTPtr name;
    ASTPtr expr;

    if (!name_p.parse(pos, name, expected))
        return false;

    if (!s_check.ignore(pos, expected))
        return false;

    if (!expression_p.parse(pos, expr, expected))
        return false;

    auto constraint = std::make_shared<ASTConstraintDeclaration>();
    constraint->name = name->as<ASTIdentifier &>().name();
    constraint->set(constraint->expr, expr);
    node = constraint;

    return true;
}


bool ParserProjectionDeclaration::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserIdentifier name_p;
    ParserProjectionSelectQuery query_p(dt);
    ParserToken s_lparen(TokenType::OpeningRoundBracket);
    ParserToken s_rparen(TokenType::ClosingRoundBracket);
    ASTPtr name;
    ASTPtr query;

    if (!name_p.parse(pos, name, expected))
        return false;

    if (!s_lparen.ignore(pos, expected))
        return false;

    if (!query_p.parse(pos, query, expected))
        return false;

    if (!s_rparen.ignore(pos, expected))
        return false;

    auto projection = std::make_shared<ASTProjectionDeclaration>();
    projection->name = name->as<ASTIdentifier &>().name();
    projection->set(projection->query, query);
    node = projection;

    return true;
}


bool ParserTablePropertyDeclaration::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_index("INDEX");
    ParserKeyword s_constraint("CONSTRAINT");
    ParserKeyword s_projection("PROJECTION");
    ParserKeyword s_primary_key("PRIMARY KEY");

    ParserIndexDeclaration index_p(dt);
    ParserConstraintDeclaration constraint_p(dt);
    ParserProjectionDeclaration projection_p(dt);
    ParserColumnDeclaration column_p{dt, true, true};
    ParserExpression primary_key_p(dt);

    ASTPtr new_node = nullptr;

    if (s_index.ignore(pos, expected))
    {
        if (!index_p.parse(pos, new_node, expected))
            return false;
    }
    else if (s_constraint.ignore(pos, expected))
    {
        if (!constraint_p.parse(pos, new_node, expected))
            return false;
    }
    else if (s_projection.ignore(pos, expected))
    {
        if (!projection_p.parse(pos, new_node, expected))
            return false;
    }
    else if (s_primary_key.ignore(pos, expected))
    {
        if (!primary_key_p.parse(pos, new_node, expected))
            return false;
    }
    else
    {
        if (!column_p.parse(pos, new_node, expected))
            return false;
    }

    node = new_node;
    return true;
}

bool ParserIndexDeclarationList::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    return ParserList(std::make_unique<ParserIndexDeclaration>(dt), std::make_unique<ParserToken>(TokenType::Comma), false)
            .parse(pos, node, expected);
}

bool ParserConstraintDeclarationList::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    return ParserList(std::make_unique<ParserConstraintDeclaration>(dt), std::make_unique<ParserToken>(TokenType::Comma), false)
            .parse(pos, node, expected);
}

bool ParserProjectionDeclarationList::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    return ParserList(std::make_unique<ParserProjectionDeclaration>(dt), std::make_unique<ParserToken>(TokenType::Comma), false)
            .parse(pos, node, expected);
}

bool ParserTablePropertiesDeclarationList::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ASTPtr list;
    if (!ParserList(
            std::make_unique<ParserTablePropertyDeclaration>(dt),
                    std::make_unique<ParserToken>(TokenType::Comma), false)
            .parse(pos, list, expected))
        return false;

    ASTPtr columns = std::make_shared<ASTExpressionList>();
    ASTPtr indices = std::make_shared<ASTExpressionList>();
    ASTPtr constraints = std::make_shared<ASTExpressionList>();
    ASTPtr projections = std::make_shared<ASTExpressionList>();
    ASTPtr primary_key;

    for (const auto & elem : list->children)
    {
        if (elem->as<ASTColumnDeclaration>())
            columns->children.push_back(elem);
        else if (elem->as<ASTIndexDeclaration>())
            indices->children.push_back(elem);
        else if (elem->as<ASTConstraintDeclaration>())
            constraints->children.push_back(elem);
        else if (elem->as<ASTProjectionDeclaration>())
            projections->children.push_back(elem);
        else if (elem->as<ASTIdentifier>() || elem->as<ASTFunction>())
        {
            if (primary_key)
            {
                /// Multiple primary keys are not allowed.
                return false;
            }
            primary_key = elem;
        }
        else
            return false;
    }

    auto res = std::make_shared<ASTColumns>();

    if (!columns->children.empty())
        res->set(res->columns, columns);
    if (!indices->children.empty())
        res->set(res->indices, indices);
    if (!constraints->children.empty())
        res->set(res->constraints, constraints);
    if (!projections->children.empty())
        res->set(res->projections, projections);
    if (primary_key)
        res->set(res->primary_key, primary_key);

    node = res;

    return true;
}


bool ParserStorage::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_engine("ENGINE");
    ParserToken s_eq(TokenType::Equals);
    ParserKeyword s_partition_by("PARTITION BY");
    ParserKeyword s_cluster_by("CLUSTER BY");
    ParserKeyword s_primary_key("PRIMARY KEY");
    ParserKeyword s_order_by("ORDER BY");
    ParserKeyword s_unique_key("UNIQUE KEY");
    ParserKeyword s_sample_by("SAMPLE BY");
    ParserKeyword s_ttl("TTL");
    ParserKeyword s_settings("SETTINGS");
    ParserKeyword s_comment("COMMENT");

    ParserIdentifierWithOptionalParameters ident_with_optional_params_p(dt);
    ParserExpression expression_p(dt);
    ParserClusterByElement cluster_p;
    ParserSetQuery settings_p(/* parse_only_internals_ = */ true);
    ParserTTLExpressionList parser_ttl_list(dt);
    ParserStringLiteral string_literal_parser;

    ASTPtr engine;
    ASTPtr partition_by;
    ASTPtr cluster_by;
    ASTPtr primary_key;
    ASTPtr order_by;
    ASTPtr unique_key;
    ASTPtr sample_by;
    ASTPtr ttl_table;
    ASTPtr settings;
    ASTPtr comment_expression;

    if (!s_engine.ignore(pos, expected))
        return false;

    s_eq.ignore(pos, expected);

    if (!ident_with_optional_params_p.parse(pos, engine, expected))
        return false;

    while (true)
    {
        if (!partition_by && s_partition_by.ignore(pos, expected))
        {
            if (expression_p.parse(pos, partition_by, expected))
                continue;
            else
                return false;
        }

        if (!cluster_by && s_cluster_by.ignore(pos, expected))
        {
            if (cluster_p.parse(pos, cluster_by, expected))
                continue;
            else
                return false;
        }

        if (!primary_key && s_primary_key.ignore(pos, expected))
        {
            if (expression_p.parse(pos, primary_key, expected))
                continue;
            else
                return false;
        }

        if (!order_by && s_order_by.ignore(pos, expected))
        {
            if (expression_p.parse(pos, order_by, expected))
                continue;
            else
                return false;
        }


        if (!unique_key && s_unique_key.ignore(pos, expected))
        {
            if (expression_p.parse(pos, unique_key, expected))
                continue;
            else
                return false;
        }

        if (!sample_by && s_sample_by.ignore(pos, expected))
        {
            if (expression_p.parse(pos, sample_by, expected))
                continue;
            else
                return false;
        }

        if (!ttl_table && s_ttl.ignore(pos, expected))
        {
            if (parser_ttl_list.parse(pos, ttl_table, expected))
                continue;
            else
                return false;
        }

        if (s_settings.ignore(pos, expected))
        {
            if (!settings_p.parse(pos, settings, expected))
                return false;
        }

        if (s_comment.ignore(pos, expected))
        {
            /// should be followed by a string literal
            if (!string_literal_parser.parse(pos, comment_expression, expected))
                return false;
        }

        break;
    }

    auto storage = std::make_shared<ASTStorage>();
    storage->set(storage->engine, engine);
    storage->set(storage->partition_by, partition_by);
    storage->set(storage->cluster_by, cluster_by);
    storage->set(storage->primary_key, primary_key);
    storage->set(storage->order_by, order_by);
    storage->set(storage->unique_key, unique_key);
    storage->set(storage->sample_by, sample_by);
    storage->set(storage->ttl_table, ttl_table);

    storage->set(storage->settings, settings);

    storage->set(storage->comment, comment_expression);

    node = storage;
    return true;
}


bool ParserCreateTableQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_create("CREATE");
    ParserKeyword s_attach("ATTACH");
    ParserKeyword s_replace("REPLACE");
    ParserKeyword s_or_replace("OR REPLACE");
    ParserKeyword s_temporary("TEMPORARY");
    ParserKeyword s_table("TABLE");
    ParserKeyword s_if_not_exists("IF NOT EXISTS");
    ParserCompoundIdentifier table_name_p(true);
    ParserKeyword s_from("FROM");
    ParserKeyword s_on("ON");
    ParserKeyword s_as("AS");
    ParserKeyword s_ignore("IGNORE");
    ParserKeyword s_replicated("REPLICATED");
    ParserKeyword s_async("ASYNC");
    ParserKeyword s_ttl("TTL");
    ParserToken s_dot(TokenType::Dot);
    ParserToken s_lparen(TokenType::OpeningRoundBracket);
    ParserToken s_rparen(TokenType::ClosingRoundBracket);
    ParserStorage storage_p(dt);
    ParserIdentifier name_p;
    ParserTablePropertiesDeclarationList table_properties_p(dt);
    ParserSelectWithUnionQuery select_p(dt);
    ParserFunction table_function_p(dt);
    ParserNameList names_p(dt);

    ASTPtr table;
    ASTPtr columns_list;
    ASTPtr storage;
    ASTPtr as_database;
    ASTPtr as_table;
    ASTPtr as_table_function;
    ASTPtr select;
    ASTPtr from_path;

    String cluster_str;
    bool attach = false;
    bool replace = false;
    bool or_replace = false;
    bool if_not_exists = false;
    bool is_temporary = false;
    bool ignore_replicated = false;
    bool ignore_async = false;
    bool ignore_ttl = false;

    if (s_create.ignore(pos, expected))
    {
        if (s_or_replace.ignore(pos, expected))
            replace = or_replace = true;
    }
    else if (s_attach.ignore(pos, expected))
        attach = true;
    else if (s_replace.ignore(pos, expected))
        replace = true;
    else
        return false;


    if (!replace && !or_replace && s_temporary.ignore(pos, expected))
    {
        is_temporary = true;
    }
    if (!s_table.ignore(pos, expected))
        return false;

    if (!replace && !or_replace && s_if_not_exists.ignore(pos, expected))
        if_not_exists = true;

    if (!table_name_p.parse(pos, table, expected))
        return false;
    tryRewriteCnchDatabaseName(table, pos.getContext());


    if (attach && s_from.ignore(pos, expected))
    {
        ParserLiteral from_path_p(dt);
        if (!from_path_p.parse(pos, from_path, expected))
            return false;
    }

    if (s_on.ignore(pos, expected))
    {
        if (!ASTQueryWithOnCluster::parse(pos, cluster_str, expected))
            return false;
    }

    auto table_id = table->as<ASTTableIdentifier>()->getTableId();

    // Shortcut for ATTACH a previously detached table
    bool short_attach = attach && !from_path;
    if (short_attach && (!pos.isValid() || pos.get().type == TokenType::Semicolon))
    {
        auto query = std::make_shared<ASTCreateQuery>();
        node = query;

        query->attach = attach;
        query->if_not_exists = if_not_exists;
        query->cluster = cluster_str;

        query->database = table_id.database_name;
        query->table = table_id.table_name;
        query->uuid = table_id.uuid;

        return true;
    }

    /// List of columns.
    if (s_lparen.ignore(pos, expected))
    {
        if (!table_properties_p.parse(pos, columns_list, expected))
            return false;

        if (!s_rparen.ignore(pos, expected))
            return false;

        auto storage_parse_result = storage_p.parse(pos, storage, expected);

        if (storage_parse_result && s_as.ignore(pos, expected))
        {
            if (!select_p.parse(pos, select, expected))
                return false;
        }

        if (!storage_parse_result && !is_temporary)
        {
            if (!s_as.ignore(pos, expected))
                return false;
            if (!table_function_p.parse(pos, as_table_function, expected))
            {
                return false;
            }
        }
    }
    /** Create queries without list of columns:
      *  - CREATE|ATTACH TABLE ... AS ...
      *  - CREATE|ATTACH TABLE ... ENGINE = engine
      */
    else
    {
        storage_p.parse(pos, storage, expected);

        if (s_as.ignore(pos, expected) && !select_p.parse(pos, select, expected)) /// AS SELECT ...
        {
            /// ENGINE can not be specified for table functions.
            if (storage || !table_function_p.parse(pos, as_table_function, expected))
            {
                /// AS [db.]table
                if (!name_p.parse(pos, as_table, expected))
                    return false;

                if (s_dot.ignore(pos, expected))
                {
                    as_database = as_table;
                    tryRewriteCnchDatabaseName(as_database, pos.getContext());
                    if (!name_p.parse(pos, as_table, expected))
                        return false;
                }

                /// Optional - IGNORE REPLCIATE can be specified
                if (s_ignore.ignore(pos, expected))
                {
                    bool option = false;
                    do
                    {
                        option = false;
                        bool temp = s_replicated.ignore(pos, expected);
                        if (temp) option = ignore_replicated = true;
                        temp = s_async.ignore(pos, expected);
                        if (temp) option = ignore_async = true;
                        temp = s_ttl.ignore(pos, expected);
                        if (temp) option = ignore_ttl = true;

                    } while (option);

                    if (!ignore_replicated && !ignore_async && !ignore_ttl)
                        return false;
                }

                /// Optional - ENGINE can be specified.
                if (!storage)
                    storage_p.parse(pos, storage, expected);
            }
        }
    }


    auto query = std::make_shared<ASTCreateQuery>();
    node = query;

    if (as_table_function)
        query->as_table_function = as_table_function;

    query->attach = attach;
    query->replace_table = replace;
    query->create_or_replace = or_replace;
    query->if_not_exists = if_not_exists;
    query->temporary = is_temporary;
    query->ignore_replicated = ignore_replicated;
    query->ignore_async = ignore_async;
    query->ignore_ttl = ignore_ttl;

    query->database = table_id.database_name;
    query->table = table_id.table_name;
    query->uuid = table_id.uuid;
    query->cluster = cluster_str;

    query->set(query->columns_list, columns_list);
    query->set(query->storage, storage);

    if (query->storage && query->columns_list && query->columns_list->primary_key)
    {
        if (query->storage->primary_key)
        {
            throw Exception("Multiple primary keys are not allowed.", ErrorCodes::BAD_ARGUMENTS);
        }
        query->storage->primary_key = query->columns_list->primary_key;
    }

    tryGetIdentifierNameInto(as_database, query->as_database);
    tryGetIdentifierNameInto(as_table, query->as_table);
    query->set(query->select, select);

    if (from_path)
        query->attach_from_path = from_path->as<ASTLiteral &>().value.get<String>();

    return true;
}

bool ParserCreateLiveViewQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_create("CREATE");
    ParserKeyword s_attach("ATTACH");
    ParserKeyword s_if_not_exists("IF NOT EXISTS");
    ParserCompoundIdentifier table_name_p(true);
    ParserKeyword s_as("AS");
    ParserKeyword s_view("VIEW");
    ParserKeyword s_live("LIVE");
    ParserToken s_dot(TokenType::Dot);
    ParserToken s_lparen(TokenType::OpeningRoundBracket);
    ParserToken s_rparen(TokenType::ClosingRoundBracket);
    ParserTablePropertiesDeclarationList table_properties_p(dt);
    ParserSelectWithUnionQuery select_p(dt);

    ASTPtr table;
    ASTPtr to_table;
    ASTPtr columns_list;
    ASTPtr as_database;
    ASTPtr as_table;
    ASTPtr select;
    ASTPtr live_view_timeout;
    ASTPtr live_view_periodic_refresh;

    String cluster_str;
    bool attach = false;
    bool if_not_exists = false;
    bool with_and = false;
    bool with_timeout = false;
    bool with_periodic_refresh = false;

    if (!s_create.ignore(pos, expected))
    {
        if (s_attach.ignore(pos, expected))
            attach = true;
        else
            return false;
    }

    if (!s_live.ignore(pos, expected))
        return false;

    if (!s_view.ignore(pos, expected))
       return false;

    if (s_if_not_exists.ignore(pos, expected))
       if_not_exists = true;

    if (!table_name_p.parse(pos, table, expected))
        return false;
    tryRewriteCnchDatabaseName(table, pos.getContext());

    if (ParserKeyword{"WITH"}.ignore(pos, expected))
    {
        if (ParserKeyword{"TIMEOUT"}.ignore(pos, expected))
        {
            if (!ParserNumber{dt}.parse(pos, live_view_timeout, expected))
            {
                live_view_timeout = std::make_shared<ASTLiteral>(static_cast<UInt64>(DEFAULT_TEMPORARY_LIVE_VIEW_TIMEOUT_SEC));
            }

            /// Optional - AND
            if (ParserKeyword{"AND"}.ignore(pos, expected))
                with_and = true;

            with_timeout = true;
        }

        if (ParserKeyword{"REFRESH"}.ignore(pos, expected) || ParserKeyword{"PERIODIC REFRESH"}.ignore(pos, expected))
        {
            if (!ParserNumber{dt}.parse(pos, live_view_periodic_refresh, expected))
                live_view_periodic_refresh = std::make_shared<ASTLiteral>(static_cast<UInt64>(DEFAULT_PERIODIC_LIVE_VIEW_REFRESH_SEC));

            with_periodic_refresh = true;
        }

        else if (with_and)
            return false;

        if (!with_timeout && !with_periodic_refresh)
            return false;
    }

    if (ParserKeyword{"ON"}.ignore(pos, expected))
    {
        if (!ASTQueryWithOnCluster::parse(pos, cluster_str, expected))
            return false;
    }

    // TO [db.]table
    if (ParserKeyword{"TO"}.ignore(pos, expected))
    {
        if (!table_name_p.parse(pos, to_table, expected))
            return false;
        tryRewriteCnchDatabaseName(to_table, pos.getContext());
    }

    /// Optional - a list of columns can be specified. It must fully comply with SELECT.
    if (s_lparen.ignore(pos, expected))
    {
        if (!table_properties_p.parse(pos, columns_list, expected))
            return false;

        if (!s_rparen.ignore(pos, expected))
            return false;
    }

    /// AS SELECT ...
    if (!s_as.ignore(pos, expected))
        return false;

    if (!select_p.parse(pos, select, expected))
        return false;


    auto query = std::make_shared<ASTCreateQuery>();
    node = query;

    query->attach = attach;
    query->if_not_exists = if_not_exists;
    query->is_live_view = true;

    auto table_id = table->as<ASTTableIdentifier>()->getTableId();
    query->database = table_id.database_name;
    query->table = table_id.table_name;
    query->uuid = table_id.uuid;
    query->cluster = cluster_str;

    if (to_table)
        query->to_table_id = to_table->as<ASTTableIdentifier>()->getTableId();

    query->set(query->columns_list, columns_list);

    tryGetIdentifierNameInto(as_database, query->as_database);
    tryGetIdentifierNameInto(as_table, query->as_table);
    query->set(query->select, select);

    if (live_view_timeout)
        query->live_view_timeout.emplace(live_view_timeout->as<ASTLiteral &>().value.safeGet<UInt64>());

    if (live_view_periodic_refresh)
        query->live_view_periodic_refresh.emplace(live_view_periodic_refresh->as<ASTLiteral &>().value.safeGet<UInt64>());

    return true;
}

bool ParserCreateDatabaseQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_create("CREATE");
    ParserKeyword s_attach("ATTACH");
    ParserKeyword s_database("DATABASE");
    ParserKeyword s_if_not_exists("IF NOT EXISTS");
    ParserStorage storage_p(dt);
    ParserIdentifier name_p;

    ASTPtr database;
    ASTPtr storage;
    UUID uuid = UUIDHelpers::Nil;

    String cluster_str;
    bool attach = false;
    bool if_not_exists = false;

    if (!s_create.ignore(pos, expected))
    {
        if (s_attach.ignore(pos, expected))
            attach = true;
        else
            return false;
    }

    if (!s_database.ignore(pos, expected))
        return false;

    if (s_if_not_exists.ignore(pos, expected))
        if_not_exists = true;

    if (!name_p.parse(pos, database, expected))
        return false;
    tryRewriteCnchDatabaseName(database, pos.getContext());

    if (ParserKeyword("UUID").ignore(pos, expected))
    {
        ParserStringLiteral uuid_p;
        ASTPtr ast_uuid;
        if (!uuid_p.parse(pos, ast_uuid, expected))
            return false;
        uuid = parseFromString<UUID>(ast_uuid->as<ASTLiteral>()->value.get<String>());
    }

    if (ParserKeyword{"ON"}.ignore(pos, expected))
    {
        if (!ASTQueryWithOnCluster::parse(pos, cluster_str, expected))
            return false;
    }

    storage_p.parse(pos, storage, expected);


    auto query = std::make_shared<ASTCreateQuery>();
    node = query;

    query->attach = attach;
    query->if_not_exists = if_not_exists;

    tryGetIdentifierNameInto(database, query->database);
    query->uuid = uuid;
    query->cluster = cluster_str;

    query->set(query->storage, storage);

    return true;
}

bool ParserCreateViewQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_create("CREATE");
    ParserKeyword s_attach("ATTACH");
    ParserKeyword s_if_not_exists("IF NOT EXISTS");
    ParserCompoundIdentifier table_name_p(true);
    ParserKeyword s_as("AS");
    ParserKeyword s_view("VIEW");
    ParserKeyword s_materialized("MATERIALIZED");
    ParserKeyword s_populate("POPULATE");
    ParserKeyword s_or_replace("OR REPLACE");
    ParserToken s_dot(TokenType::Dot);
    ParserToken s_lparen(TokenType::OpeningRoundBracket);
    ParserToken s_rparen(TokenType::ClosingRoundBracket);
    ParserStorage storage_p(dt);
    ParserIdentifier name_p;
    ParserTablePropertiesDeclarationList table_properties_p(dt);
    ParserSelectWithUnionQuery select_p(dt);
    ParserNameList names_p(dt);

    ASTPtr table;
    ASTPtr to_table;
    ASTPtr to_inner_uuid;
    ASTPtr columns_list;
    ASTPtr storage;
    ASTPtr as_database;
    ASTPtr as_table;
    ASTPtr select;

    String cluster_str;
    bool attach = false;
    bool if_not_exists = false;
    bool is_ordinary_view = false;
    bool is_materialized_view = false;
    bool is_populate = false;
    bool replace_view = false;

    if (!s_create.ignore(pos, expected))
    {
        if (s_attach.ignore(pos, expected))
            attach = true;
        else
            return false;
    }

    /// VIEW or MATERIALIZED VIEW
    if (s_or_replace.ignore(pos, expected))
    {
        replace_view = true;
    }

    if (!replace_view && s_materialized.ignore(pos, expected))
    {
        is_materialized_view = true;
    }
    else
        is_ordinary_view = true;

    if (!s_view.ignore(pos, expected))
        return false;

    if (!replace_view && s_if_not_exists.ignore(pos, expected))
        if_not_exists = true;

    if (!table_name_p.parse(pos, table, expected))
        return false;
    tryRewriteCnchDatabaseName(table, pos.getContext());

    if (ParserKeyword{"ON"}.ignore(pos, expected))
    {
        if (!ASTQueryWithOnCluster::parse(pos, cluster_str, expected))
            return false;
    }


    if (ParserKeyword{"TO INNER UUID"}.ignore(pos, expected))
    {
        ParserLiteral literal_p(dt);
        if (!literal_p.parse(pos, to_inner_uuid, expected))
            return false;
    }
    else if (ParserKeyword{"TO"}.ignore(pos, expected))
    {
        // TO [db.]table
        if (!table_name_p.parse(pos, to_table, expected))
            return false;
        tryRewriteCnchDatabaseName(to_table, pos.getContext());
    }

    /// Optional - a list of columns can be specified. It must fully comply with SELECT.
    if (s_lparen.ignore(pos, expected))
    {
        if (!table_properties_p.parse(pos, columns_list, expected))
            return false;

        if (!s_rparen.ignore(pos, expected))
            return false;
    }

    if (is_materialized_view && !to_table)
    {
        /// Internal ENGINE for MATERIALIZED VIEW must be specified.
        if (!storage_p.parse(pos, storage, expected))
            return false;

        if (s_populate.ignore(pos, expected))
            is_populate = true;
    }

    /// AS SELECT ...
    if (!s_as.ignore(pos, expected))
        return false;

    if (!select_p.parse(pos, select, expected))
        return false;


    auto query = std::make_shared<ASTCreateQuery>();
    node = query;

    query->attach = attach;
    query->if_not_exists = if_not_exists;
    query->is_ordinary_view = is_ordinary_view;
    query->is_materialized_view = is_materialized_view;
    query->is_populate = is_populate;
    query->replace_view = replace_view;

    auto table_id = table->as<ASTTableIdentifier>()->getTableId();
    query->database = table_id.database_name;
    query->table = table_id.table_name;
    query->uuid = table_id.uuid;
    query->cluster = cluster_str;

    if (to_table)
        query->to_table_id = to_table->as<ASTTableIdentifier>()->getTableId();
    if (to_inner_uuid)
        query->to_inner_uuid = parseFromString<UUID>(to_inner_uuid->as<ASTLiteral>()->value.get<String>());

    query->set(query->columns_list, columns_list);
    query->set(query->storage, storage);

    tryGetIdentifierNameInto(as_database, query->as_database);
    tryGetIdentifierNameInto(as_table, query->as_table);
    query->set(query->select, select);

    return true;

}

bool ParserCreateDictionaryQuery::parseImpl(IParser::Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserKeyword s_create("CREATE");
    ParserKeyword s_attach("ATTACH");
    ParserKeyword s_replace("REPLACE");
    ParserKeyword s_or_replace("OR REPLACE");
    ParserKeyword s_dictionary("DICTIONARY");
    ParserKeyword s_if_not_exists("IF NOT EXISTS");
    ParserKeyword s_on("ON");
    ParserCompoundIdentifier dict_name_p(true);
    ParserToken s_left_paren(TokenType::OpeningRoundBracket);
    ParserToken s_right_paren(TokenType::ClosingRoundBracket);
    ParserToken s_dot(TokenType::Dot);
    ParserDictionaryAttributeDeclarationList attributes_p(dt);
    ParserDictionary dictionary_p(dt);

    bool if_not_exists = false;
    bool replace = false;
    bool or_replace = false;

    ASTPtr name;
    ASTPtr attributes;
    ASTPtr dictionary;
    String cluster_str;

    bool attach = false;

    if (s_create.ignore(pos, expected))
    {
        if (s_or_replace.ignore(pos, expected))
        {
            replace = true;
            or_replace = true;
        }
    }
    else if (s_attach.ignore(pos, expected))
        attach = true;
    else if (s_replace.ignore(pos, expected))
        replace = true;
    else
        return false;

    if (!s_dictionary.ignore(pos, expected))
        return false;

    if (s_if_not_exists.ignore(pos, expected))
        if_not_exists = true;

    if (!dict_name_p.parse(pos, name, expected))
        return false;
    tryRewriteCnchDatabaseName(name, pos.getContext());    

    if (s_on.ignore(pos, expected))
    {
        if (!ASTQueryWithOnCluster::parse(pos, cluster_str, expected))
            return false;
    }

    if (!attach)
    {
        if (!s_left_paren.ignore(pos, expected))
            return false;

        if (!attributes_p.parse(pos, attributes, expected))
            return false;

        if (!s_right_paren.ignore(pos, expected))
            return false;

        if (!dictionary_p.parse(pos, dictionary, expected))
            return false;
    }

    auto query = std::make_shared<ASTCreateQuery>();
    node = query;
    query->is_dictionary = true;
    query->attach = attach;
    query->create_or_replace = or_replace;
    query->replace_table = replace;

    auto dict_id = name->as<ASTTableIdentifier>()->getTableId();
    query->database = dict_id.database_name;
    query->table = dict_id.table_name;
    query->uuid = dict_id.uuid;

    query->if_not_exists = if_not_exists;
    query->set(query->dictionary_attributes_list, attributes);
    query->set(query->dictionary, dictionary);
    query->cluster = cluster_str;

    return true;
}


bool ParserCreateQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    ParserCreateTableQuery table_p(dt);
    ParserCreateDatabaseQuery database_p(dt);
    ParserCreateViewQuery view_p(dt);
    ParserCreateDictionaryQuery dictionary_p(dt);
    ParserCreateLiveViewQuery live_view_p(dt);

    return table_p.parse(pos, node, expected)
        || database_p.parse(pos, node, expected)
        || view_p.parse(pos, node, expected)
        || dictionary_p.parse(pos, node, expected)
        || live_view_p.parse(pos, node, expected);
}

}
