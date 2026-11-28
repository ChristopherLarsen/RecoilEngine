/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Trace core: the trace file (LUA-FREE H02). One file per system: what the native Core was fed and what the Lua it
// replaces answered, step by step, so the answer can be replayed against Core without the engine. No engine include.
//
// Bytes (little endian, as the machines that write and read them):
//   "KT1T", u32 version, string system, string engine, string scene, u32 steps per second
//   then records, each: u8 kind, then
//     Column  u16 column, u8 flags (1: an input, 2: ordered, 4: not compared), string name   before its first row
//     Row     u16 column, i32 step, u32 length, bytes
//     End     u32 rows                                                                       a complete file ends here
//   string: u16 length, bytes
// An output row's bytes: 3 i32 key, then float values (this engine's Lua number is float). An input row's bytes are the
// system's own, written with Bytes and read with ByteReader.
// Rows are in the order given. Steps never go back.

#ifndef KT1_TRACE_FILE_H
#define KT1_TRACE_FILE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace KT1::Core::Trace {

constexpr uint32_t VERSION = 1;              // DESIGN: the layout above
constexpr size_t MAX_COLUMNS = 1024;         // DESIGN: columns one system may declare
constexpr size_t MAX_ROW_BYTES = 1 << 24;    // DESIGN: a longer row is a damaged file
constexpr size_t KEY_IDS = 3;                // as the check helper's keys

enum class Record : uint8_t { Column = 1, Row = 2, End = 3 };
enum ColumnFlags : uint8_t { INPUT = 1, ORDERED = 2, NOT_COMPARED = 4 };

using Key = std::array<int32_t, KEY_IDS>;

// A growing byte string. Plain values only: no pointer, no padding.
class Bytes {
public:
	template<class T> void Put(const T& v) {
		static_assert(std::is_trivially_copyable_v<T>, "a trace holds plain values");
		const size_t at = data.size();
		data.resize(at + sizeof(T));
		std::memcpy(data.data() + at, &v, sizeof(T));
	}
	void PutString(std::string_view s) {
		const uint16_t n = static_cast<uint16_t>(s.size() < UINT16_MAX ? s.size() : UINT16_MAX);
		Put(n);
		data.insert(data.end(), s.begin(), s.begin() + n);
	}
	void Append(std::span<const uint8_t> b) { data.insert(data.end(), b.begin(), b.end()); }
	std::span<const uint8_t> View() const { return data; }
	size_t Size() const { return data.size(); }
	void Clear() { data.clear(); }

private:
	std::vector<uint8_t> data;
};

// Reads what Bytes wrote. Reading past the end gives zeroes and sets Failed().
class ByteReader {
public:
	ByteReader() = default;
	explicit ByteReader(std::span<const uint8_t> data) : data(data) {}

	template<class T> T Get() {
		static_assert(std::is_trivially_copyable_v<T>, "a trace holds plain values");
		T v{};
		if (data.size() - at < sizeof(T)) {
			failed = true;
			at = data.size();
			return v;
		}
		std::memcpy(&v, data.data() + at, sizeof(T));
		at += sizeof(T);
		return v;
	}
	std::string GetString() {
		const uint16_t n = Get<uint16_t>();
		const std::span<const uint8_t> b = Take(n);
		return std::string(b.begin(), b.end());
	}
	std::span<const uint8_t> Take(size_t n) {
		if (data.size() - at < n) {
			failed = true;
			at = data.size();
			return {};
		}
		const std::span<const uint8_t> out = data.subspan(at, n);
		at += n;
		return out;
	}
	size_t Left() const { return data.size() - at; }
	bool Failed() const { return failed; }

private:
	std::span<const uint8_t> data;
	size_t at = 0;
	bool failed = false;
};

struct Header {
	std::string system, engine, scene;
	uint32_t stepsPerSecond = 0;
};

struct Column {
	std::string name;
	uint8_t flags = 0;

	bool IsInput() const { return (flags & INPUT) != 0; }
	bool Ordered() const { return (flags & ORDERED) != 0; }
	bool Compared() const { return (flags & NOT_COMPARED) == 0; }
};

// Builds one system's trace into a buffer the caller drains (Pending, Drained).
class Writer {
public:
	void Begin(const Header& h) {
		columns.clear();
		out.Clear();
		rows = 0;
		for (const char c : {'K', 'T', '1', 'T'}) out.Put(c);
		out.Put(VERSION);
		out.PutString(h.system);
		out.PutString(h.engine);
		out.PutString(h.scene);
		out.Put(h.stepsPerSecond);
	}

	// The column of this name, declared on first use with these flags; -1: too many columns.
	int ColumnOf(std::string_view name, uint8_t flags) {
		for (size_t i = 0; i < columns.size(); ++i)
			if (columns[i].name == name && (columns[i].flags & INPUT) == (flags & INPUT)) return static_cast<int>(i);
		if (columns.size() >= MAX_COLUMNS) return -1;
		columns.push_back(Column{std::string(name), flags});
		out.Put(Record::Column);
		out.Put(static_cast<uint16_t>(columns.size() - 1));
		out.Put(flags);
		out.PutString(name);
		return static_cast<int>(columns.size()) - 1;
	}

	void Row(int column, int32_t step, std::span<const uint8_t> bytes) {
		if (column < 0 || bytes.size() > MAX_ROW_BYTES) return;
		out.Put(Record::Row);
		out.Put(static_cast<uint16_t>(column));
		out.Put(step);
		out.Put(static_cast<uint32_t>(bytes.size()));
		out.Append(bytes);
		++rows;
	}

	// an output row: the key, then the values
	void Output(std::string_view name, uint8_t flags, int32_t step, const Key& key, std::span<const float> values) {
		scratch.Clear();
		for (const int32_t id : key) scratch.Put(id);
		for (const float v : values) scratch.Put(v);
		Row(ColumnOf(name, static_cast<uint8_t>(flags & ~INPUT)), step, scratch.View());
	}

	void End() {
		out.Put(Record::End);
		out.Put(rows);
	}

	std::span<const uint8_t> Pending() const { return out.View(); }
	void Drained() { out.Clear(); }
	uint32_t Rows() const { return rows; }

private:
	std::vector<Column> columns;
	Bytes out, scratch;
	uint32_t rows = 0;
};

// One row as read: its column, step and bytes (a view into the trace).
struct RowView {
	int column = -1;
	int32_t step = 0;
	std::span<const uint8_t> bytes;
};

// Walks a whole trace held in memory.
class Reader {
public:
	// false: not a trace, or a version this reader does not know
	bool Open(std::span<const uint8_t> data) {
		in = ByteReader(data);
		columns.clear();
		complete = false;
		rows = 0;
		error.clear();
		for (const char c : {'K', 'T', '1', 'T'})
			if (in.Get<char>() != c) return Fail("not a trace file");
		if (in.Get<uint32_t>() != VERSION) return Fail("unknown trace version");
		header.system = in.GetString();
		header.engine = in.GetString();
		header.scene = in.GetString();
		header.stepsPerSecond = in.Get<uint32_t>();
		return in.Failed() ? Fail("short header") : true;
	}

	// The next row. false: the end (Complete() says whether the file ended as it should) or an error.
	bool Next(RowView& row) {
		while (error.empty() && in.Left() > 0) {
			const Record kind = in.Get<Record>();
			if (kind == Record::Column) {
				if (!ReadColumn()) return false;
			} else if (kind == Record::Row) {
				return ReadRow(row);
			} else if (kind == Record::End) {
				complete = (in.Get<uint32_t>() == rows) && !in.Failed() && in.Left() == 0;
				if (!complete) Fail("bad end record");
				return false;
			} else {
				return Fail("unknown record");
			}
		}
		return false;
	}

	const Header& Head() const { return header; }
	const std::vector<Column>& Columns() const { return columns; }
	bool Complete() const { return complete; }
	const std::string& Error() const { return error; }
	uint32_t Rows() const { return rows; }

private:
	bool Fail(const char* why) {
		error = why;
		return false;
	}
	bool ReadColumn() {
		const uint16_t id = in.Get<uint16_t>();
		Column c;
		c.flags = in.Get<uint8_t>();
		c.name = in.GetString();
		if (in.Failed() || id != columns.size() || columns.size() >= MAX_COLUMNS) return Fail("bad column record");
		columns.push_back(c);
		return true;
	}
	bool ReadRow(RowView& row) {
		const uint16_t id = in.Get<uint16_t>();
		row.step = in.Get<int32_t>();
		const uint32_t n = in.Get<uint32_t>();
		if (n > MAX_ROW_BYTES) return Fail("bad row length");
		row.bytes = in.Take(n);
		if (in.Failed() || id >= columns.size()) return Fail("short or unknown row");
		row.column = id;
		++rows;
		return true;
	}

	ByteReader in;
	Header header;
	std::vector<Column> columns;
	std::string error;
	uint32_t rows = 0;
	bool complete = false;
};

} // namespace KT1::Core::Trace

#endif
