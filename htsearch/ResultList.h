//
// ResultList.h
//
// ResultList: A Dictionary indexed on the document id that holds
//             documents found for a search.
//
// Part of the ht://Dig package   <http://www.htdig.org/>
// Copyright (c) 1995-2003 The ht://Dig Group
// For copyright details, see the file COPYING in your distribution
// or the GNU Library General Public License (LGPL) version 2 or later
// <http://www.gnu.org/copyleft/lgpl.html>
//
// $Id: ResultList.h,v 1.7 2003/06/24 19:58:07 nealr Exp $
//

#ifndef _ResultList_h_
#define _ResultList_h_

#include "Dictionary.h"
#include "DocMatch.h"
#include "HtVector.h"

class ResultList : public Dictionary
{
public:
    ResultList();
    ~ResultList();
	ResultList(const ResultList &);

    void		add(DocMatch *);
    void		remove(int id);
    DocMatch		*find(int id) const;
    DocMatch		*find(char *id) const;
    int			exists(int id) const;

    HtVector		*elements();

    void		SetWeight(double weight);
    bool		IsIgnore() const { return isIgnore != 0; }
    void		Ignore() { isIgnore = 1; }

    void		Dump() const;
//private:

    int			isIgnore;
};

#endif


