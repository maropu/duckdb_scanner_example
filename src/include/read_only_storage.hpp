//===----------------------------------------------------------------------===//
//                         DuckDB
//
// read_only_storage.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/schema_catalog_entry.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/parser/parsed_data/create_schema_info.hpp"
#include "duckdb/parser/parsed_data/create_table_info.hpp"
#include "duckdb/storage/database_size.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/transaction/transaction_manager.hpp"
#include "duckdb/transaction/transaction.hpp"

namespace duckdb {

class ReadOnlyCatalog : public Catalog {
public:
	ReadOnlyCatalog(AttachedDatabase &db_p) : Catalog(db_p) {
	}

	// Used by ensuring that we did not already attach a database with the same path.
	// Because path conflicts don't matter in read-only cases,
	// ISTM that it is okay to return true here.
	bool InMemory() override {
		return true;
	}

	string GetDBPath() override {
		return nullptr;
	}

	virtual idx_t GetDataBaseByteSize(ClientContext &context) = 0;

	DatabaseSize GetDatabaseSize(ClientContext &context) override {
		DatabaseSize size;
		size.free_blocks = 0;
		size.total_blocks = 0;
		size.used_blocks = 0;
		size.wal_size = 0;
		size.block_size = 0;
		size.bytes = GetDataBaseByteSize(context);
		return size;
	}

	// This class assumes a simple storage supports a table entry only
	CatalogLookupBehavior CatalogTypeLookupRule(CatalogType type) const override {
		switch (type) {
		case CatalogType::TABLE_ENTRY:
			return CatalogLookupBehavior::STANDARD;
		default:
			// unsupported type (e.g. view, index, scalar functions, aggregates, ...)
			return CatalogLookupBehavior::NEVER_LOOKUP;
		}
	}

	virtual void Initialize(bool load_builtin) override = 0;
	virtual void ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) override = 0;
	virtual optional_ptr<SchemaCatalogEntry> GetSchema(CatalogTransaction transaction, const string &schema_name,
													   OnEntryNotFound if_not_found,
													   QueryErrorContext error_context = QueryErrorContext()) override = 0;

	optional_ptr<CatalogEntry> CreateSchema(CatalogTransaction transaction, CreateSchemaInfo &info) override {
		throw NotImplementedException("CreateSchema is not supported for read-only storage");
	}

	unique_ptr<PhysicalOperator> PlanInsert(ClientContext &context, LogicalInsert &op,
											unique_ptr<PhysicalOperator> plan) override {
		throw NotImplementedException("PlanInsert is not supported for read-only storage");
	}

	unique_ptr<PhysicalOperator> PlanCreateTableAs(ClientContext &context, LogicalCreateTable &op,
												   unique_ptr<PhysicalOperator> plan) override {
		throw NotImplementedException("PlanCreateTableAs is not supported for read-only storage");
	}

	unique_ptr<PhysicalOperator> PlanDelete(ClientContext &context, LogicalDelete &op,
											unique_ptr<PhysicalOperator> plan) override {
		throw NotImplementedException("PlanDelete is not supported for read-only storage");
	}

	unique_ptr<PhysicalOperator> PlanUpdate(ClientContext &context, LogicalUpdate &op,
											unique_ptr<PhysicalOperator> plan) override {
		throw NotImplementedException("PlanUpdate is not supported for read-only storage");
	}

	unique_ptr<LogicalOperator> BindCreateIndex(Binder &binder, CreateStatement &stmt, TableCatalogEntry &table,
												unique_ptr<LogicalOperator> plan) override {
		throw NotImplementedException("BindCreateIndex is not supported for read-only storage");
	}

private:
	void DropSchema(ClientContext &context, DropInfo &info) override {
		throw NotImplementedException("DropSchema is not supported for read-only storage");
	}
};

class ReadOnlyTableCatalogEntry : public TableCatalogEntry {
public:
	ReadOnlyTableCatalogEntry(Catalog &catalog_p, SchemaCatalogEntry &schema_p, CreateTableInfo &info_p)
		: TableCatalogEntry(catalog_p, schema_p, info_p) {
	};

	virtual unique_ptr<BaseStatistics> GetStatistics(ClientContext &context, column_t column_id) override = 0;
	virtual TableFunction GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) override = 0;
	virtual TableStorageInfo GetStorageInfo(ClientContext &context) override = 0;
};

class ReadOnlySchemaCatalogEntry : public SchemaCatalogEntry {
public:
	ReadOnlySchemaCatalogEntry(Catalog &catalog_p, CreateSchemaInfo &info_p)
		: SchemaCatalogEntry(catalog_p, info_p) {
	}

	bool CatalogTypeIsSupported(CatalogType type) {
		return type == CatalogType::TABLE_ENTRY;
	}

	virtual void Scan(ClientContext &context, CatalogType type,
					  const std::function<void(CatalogEntry &)> &callback) override = 0;
	virtual optional_ptr<CatalogEntry> GetEntry(CatalogTransaction transaction, CatalogType type,
												const string &name_p) override = 0;

	optional_ptr<CatalogEntry> CreateTable(CatalogTransaction transaction, BoundCreateTableInfo &info) override {
		throw NotImplementedException("CreateTable is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreateFunction(CatalogTransaction transaction, CreateFunctionInfo &info) override {
		throw NotImplementedException("CreateFunction is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreateIndex(CatalogTransaction transaction, CreateIndexInfo &info,
										   TableCatalogEntry &table) override {
		throw NotImplementedException("CreateIndex is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreateView(CatalogTransaction transaction, CreateViewInfo &info) override {
		throw NotImplementedException("CreateView is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreateSequence(CatalogTransaction transaction, CreateSequenceInfo &info) override {
		throw NotImplementedException("CreateSequence is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreateTableFunction(CatalogTransaction transaction,
												   CreateTableFunctionInfo &info) override {
		throw NotImplementedException("CreateTableFunction is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreateCopyFunction(CatalogTransaction transaction,
												  CreateCopyFunctionInfo &info) override {
		throw NotImplementedException("CreateCopyFunction is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreatePragmaFunction(CatalogTransaction transaction,
													CreatePragmaFunctionInfo &info) override {
		throw NotImplementedException("CreatePragmaFunction is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreateCollation(CatalogTransaction transaction, CreateCollationInfo &info) override {
		throw NotImplementedException("CreateCollation is not supported for read-only storage");
	}

	optional_ptr<CatalogEntry> CreateType(CatalogTransaction transaction, CreateTypeInfo &info) override {
		throw NotImplementedException("CreateType is not supported for read-only storage");
	}

	void Alter(CatalogTransaction transaction, AlterInfo &info) override {
		throw NotImplementedException("Alter is not supported for read-only storage");
	}

	void Scan(CatalogType type, const std::function<void(CatalogEntry &)> &callback) override {
		throw NotImplementedException("Scan without context not supported");
	}

	void DropEntry(ClientContext &context, DropInfo &info) override {
		throw NotImplementedException("DropEntry is not supported for read-only storage");
	}
};

class ReadOnlyTransaction : public Transaction {
public:
	ReadOnlyTransaction(TransactionManager &manager_p, ClientContext &context_p)
		: Transaction(manager_p, context_p) {
	}

	DUCKDB_API bool IsReadOnly() {
		return true;
	}
};

class NoOpTransactionManager : public TransactionManager {
public:
	NoOpTransactionManager(AttachedDatabase &db_p) : TransactionManager(db_p) {
	}

	static unique_ptr<TransactionManager> Create(StorageExtensionInfo *storage_info, AttachedDatabase &db,
												 Catalog &catalog) {
		return make_uniq<NoOpTransactionManager>(db);
	}

	Transaction &StartTransaction(ClientContext &context) override {
		auto transaction = make_uniq<ReadOnlyTransaction>(*this, context);
		auto &result = *transaction;
		lock_guard<mutex> l(transaction_lock);
		transactions[result] = std::move(transaction);
		return result;
	}

	ErrorData CommitTransaction(ClientContext &context, Transaction &transaction) override {
		auto &tx = transaction.Cast<ReadOnlyTransaction>();
		lock_guard<mutex> l(transaction_lock);
		transactions.erase(tx);
		return ErrorData();
	}

	void RollbackTransaction(Transaction &transaction) override {
		auto &tx = transaction.Cast<ReadOnlyTransaction>();
		lock_guard<mutex> l(transaction_lock);
		transactions.erase(tx);
	}

	void Checkpoint(ClientContext &context, bool force = false) override {
	}

private:
	mutex transaction_lock;
	reference_map_t<Transaction, unique_ptr<Transaction>> transactions;
};

class ReadOnlyStorageExtension : public StorageExtension {
public:
	ReadOnlyStorageExtension() {
		create_transaction_manager = NoOpTransactionManager::Create;
	};
};

} // namespace duckdb