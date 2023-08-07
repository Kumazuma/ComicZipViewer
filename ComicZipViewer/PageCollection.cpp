#include "framework.h"
#include "PageCollection.h"
#include "PageCollectionZip.h"
#include <wx/filename.h>

PageCollection* PageCollection::Create(const wxString& path)
{
	const wxFileName fileName(path);
	if(fileName.IsDir())
	{
		// TODO: Not implemented yet!
		return nullptr;
	}
	else if(fileName.IsFileReadable())
	{
		auto ext = fileName.GetExt();
		ext = ext.Lower();
		if(ext.EndsWith(wxS("zip")) || ext.EndsWith(wxS("cbz")))
		{
			auto collection = new PageCollectionZip(path);
			if(collection->GetPageCount() == 0)
			{
				delete collection;
				collection = nullptr;
			}

			return collection ;
		}
		else
		{
			// TODO: Not implemented yet!
		}
	}

	return nullptr;
}
