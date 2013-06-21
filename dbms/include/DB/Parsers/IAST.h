#pragma once

#include <list>
#include <sstream>

#include <Poco/SharedPtr.h>

#include <Yandex/Common.h>

#include <DB/Core/Types.h>
#include <DB/Core/Exception.h>
#include <DB/Core/ErrorCodes.h>
#include <DB/IO/WriteHelpers.h>
#include <DB/Parsers/StringRange.h>

#include <iostream>


namespace DB
{

using Poco::SharedPtr;


/** Элемент синтаксического дерева (в дальнейшем - направленного ациклического графа с элементами семантики)
  */
class IAST
{
public:
	typedef std::vector<SharedPtr<IAST> > ASTs;
	ASTs children;
	StringRange range;
	
	/** Строка с полным запросом.
	  * Этот указатель не дает ее удалить, пока range в нее ссылается.
	  */
	StringPtr query_string;

	/** Идентификатор части выражения. Используется при интерпретации, чтобы вычислять не всё выражение сразу,
	  *  а по частям (например, сначала WHERE, потом фильтрация, потом всё остальное).
	  */
	unsigned part_id;
	

	IAST() : range(NULL, NULL), part_id(0) {}
	IAST(StringRange range_) : range(range_), part_id(0) {}
	virtual ~IAST() {}

	/** Получить каноническое имя столбца, если элемент является столбцом */
	virtual String getColumnName() const { throw Exception("Trying to get name of not a column: " + getID(), ErrorCodes::NOT_A_COLUMN); }

	/** Получить алиас, если он есть, или каноническое имя столбца; если элемент является столбцом */
	virtual String getAlias() const { return getColumnName(); }
		
	/** Получить текст, который идентифицирует этот элемент. */
	virtual String getID() const = 0;

	/** Получить глубокую копию дерева. */
	virtual SharedPtr<IAST> clone() const = 0;

	/** Получить текст, который идентифицирует этот элемент и всё поддерево.
	  * Обычно он содержит идентификатор элемента и getTreeID от всех детей. 
	  */
	String getTreeID() const
	{
		std::stringstream s;
		s << getID();

		if (!children.empty())
		{
			s << "(";
			for (ASTs::const_iterator it = children.begin(); it != children.end(); ++it)
			{
				if (it != children.begin())
					s << ", ";
				s << (*it)->getTreeID();
			}
			s << ")";
		}

		return s.str();
	}

	void dumpTree(std::ostream & ostr, size_t indent = 0) const
	{
		String indent_str(indent, '-');
		ostr << indent_str << getID() << ", " << this << ", part_id = " << part_id << std::endl;
		for (ASTs::const_iterator it = children.begin(); it != children.end(); ++it)
			(*it)->dumpTree(ostr, indent + 1);
	}

	/** Проверить глубину дерева.
	  * Если задано max_depth и глубина больше - кинуть исключение.
	  */
	size_t checkDepth(size_t max_depth) const
	{
		size_t res = 0;
		for (ASTs::const_iterator it = children.begin(); it != children.end(); ++it)
			if (max_depth == 0 || (res = (*it)->checkDepth(max_depth - 1)) > max_depth - 1)
				throw Exception("AST is too deep. Maximum: " + toString(max_depth), ErrorCodes::TOO_DEEP_AST);

		return res + 1;
	}
	
	/** То же самое для общего количества элементов дерева.
	  */
	size_t checkSize(size_t max_size) const
	{
		size_t res = 1;
		for (ASTs::const_iterator it = children.begin(); it != children.end(); ++it)
			res += (*it)->checkSize(max_size);

		if (res > max_size)
			throw Exception("AST is too big. Maximum: " + toString(max_size), ErrorCodes::TOO_BIG_AST);
		
		return res;
	}
};


typedef SharedPtr<IAST> ASTPtr;
typedef std::vector<ASTPtr> ASTs;

}
