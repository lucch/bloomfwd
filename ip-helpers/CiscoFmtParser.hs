module CiscoFmtParser where

import Data.Char          (isDigit)
import System.Environment (getArgs)

main :: IO ()
main = do
    (filename:_) <- getArgs
    s <- readFile filename
    mapM_ putStrLn $ filter (any (== '/')) . lines $ s

extractPrefix :: String -> String
extractPrefix =
    takeWhile (/= ' ') . dropWhile (\c -> not (isDigit c) && c /= ':')

