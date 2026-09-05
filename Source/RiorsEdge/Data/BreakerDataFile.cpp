#include "Data/BreakerDataFile.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString BreakerDataFile::FullPath(const FString& RelativePath)
{
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativePath);
}

TSharedPtr<FJsonObject> BreakerDataFile::Load(const FString& RelativePath, FBreakerDataErrors& Errors)
{
    const FString Path = FullPath(RelativePath);

    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        Errors.Add(FString::Printf(TEXT("%s: missing or unreadable"), *Path));
        return nullptr;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        Errors.Add(FString::Printf(TEXT("%s: not a JSON object (%s)"), *Path, *Reader->GetErrorMessage()));
        return nullptr;
    }
    return Root;
}
