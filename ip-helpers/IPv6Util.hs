{-# LANGUAGE OverloadedStrings #-}

module IPv6Util where

{-|
Module: IPv6Parser
Description: This module has functions to parse IPv6 compact addresses to their
             expanded forms.
Copyright: (c) Alexandre Lucchesi, 2016
License: GPL-3
Maintainer: alexandrelucchesi@gmail.com
Stability: Experimental
Portability:

This module contains functions to parse an IPv6 address represented in the
compact form to its expanded forms. There are currently two well-known forms:
one that contains leading zeroes and another that doesn't.
 
Example:

  let compact = "2001:db8::ff00:42:8329"
  in nolz compact -- "2001:db8:0:0:0:ff00:42:8329"

  let compact = "2001:db8::ff00:42:8329"
  in lz compact -- "2001:0db8:0000:0000:0000:ff00:0042:8329"
-}
import Data.Text (Text)
import qualified Data.Text as T
import qualified Data.Text.IO as T
import System.Environment

main :: IO ()
main = do
    [addrFilePath] <- getArgs
    addrs <- T.readFile addrFilePath
    mapM_ (T.putStrLn . lz) (T.lines addrs)

-- | Expanded with no leading zeroes.
nolz :: Text -> Text
nolz s =
    let xs = T.splitOn "::" s
        ys = map (T.splitOn ":") xs
        fstPart = (`T.append` ":") $ if T.null (xs !! 0) then "0" else xs !! 0
        sndPart = T.replicate (8 - (length (head ys) + length (last ys))) "0:"
        lastPart = if length xs > 1 then xs !! 1 else ""
    in if length xs > 1
        then T.concat [fstPart, sndPart, lastPart]
        else s

-- | Expanded with leading zeroes.
lz :: Text -> Text
lz s =
    let xs = T.splitOn ":" $ nolz s
    in T.intercalate ":" $ map fillZeroes xs
  where
    fillZeroes t = 
        let l = T.length t
        in if l < 4
            then T.replicate (4 - l) "0" `T.append` t
            else t

