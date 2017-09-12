{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE CPP #-}
 
import Numeric
import Data.Bits
import Data.Char
import Data.Word
import System.IO
import Control.Monad (when)
import Data.Maybe (fromJust)
import Data.Text (Text)
import qualified Data.Text as T
import qualified Data.Text.IO as T
import qualified Data.ByteString.Lazy.Char8 as BLC
import Network.Wreq
import Control.Lens
import Data.Aeson (toJSON)
import Data.Aeson.Lens (key, nth)

-- TODO: Should probably create a data type!
type Ip = Text
type Prefix = (Ip, Int)
type FwdTable = [(Integer, Integer)]

-- | See for details: https://en.wikipedia.org/wiki/Reserved_IP_addresses
reservedPrefixes :: [(Ip, Int)]
reservedPrefixes =
    [ ("0.0.0.0"         , 8)
    , ("10.0.0.0"        , 8)
    , ("100.64.0.0"      , 10)
    , ("127.0.0.0"       , 8)
    , ("169.254.0.0"     , 16)
    , ("172.16.0.0"      , 12)
    , ("192.0.0.0"       , 24)
    , ("192.0.2.0"       , 24)
    , ("192.88.99.0"     , 24)
    , ("192.168.0.0"     , 16)
    , ("198.18.0.0"      , 15)
    , ("198.51.100.0"    , 24)
    , ("203.0.113.0"     , 24)
    , ("224.0.0.0"       , 4)
    , ("240.0.0.0"       , 4)
    , ("255.255.255.255" , 32)
    ]

-- | See for details: https://en.wikipedia.org/wiki/Reserved_IP_addresses
reservedAddrsRanges :: [(Ip, Ip)]
reservedAddrsRanges =
    [ ("0.0.0.0"         , "0.255.255.255")
    , ("10.0.0.0"        , "10.255.255.255")
    , ("100.64.0.0"      , "100.127.255.255")
    , ("127.0.0.0"       , "127.255.255.255")
    , ("169.254.0.0"     , "169.254.255.255")
    , ("172.16.0.0"      , "172.31.255.255")
    , ("192.0.0.0"       , "192.0.0.255")
    , ("192.0.2.0"       , "192.0.2.255")
    , ("192.88.99.0"     , "192.88.99.255")
    , ("192.168.0.0"     , "192.168.255.255")
    , ("198.18.0.0 "     , "198.19.255.255")
    , ("198.51.100.0"    , "198.51.100.255")
    , ("203.0.113.0"     , "203.0.113.255")
    , ("224.0.0.0"       , "239.255.255.255")
    , ("240.0.0.0"       , "255.255.255.254")
    , ("255.255.255.255" , "255.255.255.255")
    ]

ipToHexString :: Ip -> String
ipToHexString =
   concat . map (zeroFill . (flip showHex "") . read . T.unpack) . T.splitOn "."
  where
    zeroFill s = Prelude.replicate (2 - length s) '0' ++ s

ipToInt :: Ip -> Integer
ipToInt =
   fst . head . readHex . ipToHexString

rangeFromPrefix :: Prefix -> (Integer, Integer)
rangeFromPrefix (prefix, prefixLen)
  | prefixLen == 0  = (0, 0xffffffff)
  | prefixLen == 32 = (ipToInt prefix, ipToInt prefix)
  | prefixLen > 0 && prefixLen < 32 = 
    let intPrefix = ipToInt prefix
        p1 = intPrefix .&. (0xffffffff `shiftL` fromIntegral (32 - prefixLen))
        p2 = intPrefix .|. (0xffffffff `shiftR` prefixLen)
    in (p1, p2)
  | otherwise = error "Bad prefix length."

-- | Technically, the predicate `map rangeFromPrefix reservedPrefixes ==
-- reservedAddrsRanges'` should be true. However, in order to avoid redundancy,
-- sometimes the range is reported excluding addresses enclosed by other
-- prefixes. For instance, the address "255.255.255.255" is the upper bound for
-- the prefix "240.0.0.0/4", but it's not reported in Wikipedia's range because
-- that address is specifically excluded by the prefix "255.255.255.255/32".
reservedAddrsRanges' :: [(Integer, Integer)]
reservedAddrsRanges' =
    map (\(x, y) -> (ipToInt x, ipToInt y)) reservedAddrsRanges

match :: FwdTable -> Ip -> Bool
match tbl ip =
    let intIp = ipToInt ip
    in any (\(x, y) -> intIp >= x && intIp <= y) tbl

ipReserved :: Ip -> Bool
ipReserved = match reservedAddrsRanges'

-- TODO: Add prefix validation!
parsePrefix :: Text -> Maybe Prefix
parsePrefix = \t ->
    case T.break isSpace t of
        ("", _) -> Nothing
        (p, t') ->
            let (prefix, t'') = T.break (== '/') p
            in case t'' of
                "" -> return $
                    let [a, b, c, d] = map (read . T.unpack) $ T.splitOn "." prefix
                        prefixLen =
                            if d > 0 then 32
                            else if c > 0 then 24
                            else if b > 0 then 16
                            else if a > 0 then 8
                            else 0
                    in (prefix, prefixLen)
                l -> if T.head l == '/'
                        then let prefixLen = read . T.unpack $ T.tail l
                             in return (prefix, prefixLen)
                        else Nothing
    
loadPrefixes :: FilePath -> IO [Prefix]
loadPrefixes path = do
    t <- T.readFile path
    return $ map (fromJust . parsePrefix) $ T.lines t

-- | Randomly generate IP addresses with a given matching ratio, i.e., a `ratio`
-- of 0.5 implies that exactly half the addresses match at least a prefix in the
-- specified prefixes file (used to build a forwarding table). Logically, the
-- other half ends up in the default route.
genAddrs :: Integer -> Maybe (FilePath, Double) -> FilePath -> IO ()
genAddrs numAddrs maybeMatching outputFile = do
    addrs <- case maybeMatching of
            Just (pfxsFile, ratio) -> do
                let mCount = round $ fromInteger numAddrs * ratio
                    uCount = round $ fromInteger numAddrs * (1.0 - ratio)
                fwdTbl <- map rangeFromPrefix <$> loadPrefixes pfxsFile
                -- TODO: Can be run in parallel!
                ms <- gen mCount (match fwdTbl)
                us <- gen uCount (not . (match fwdTbl))
                return $ evenlyCombine ms us
            _ -> gen numAddrs (const True)
                
    withFile outputFile WriteMode $ \handle -> do
        hPutStrLn handle $ (show . length) addrs 
        T.hPutStrLn handle $ (T.strip . T.unlines) addrs
  where
    gen 0 _ = return []
    gen count pred = do
        let totalInts = count * 4
            -- 10000 is the maximum allowed quantity per request.
            num = if totalInts < 10000 then totalInts else 10000
        r <- getWith (opts num) url
        let rb = T.pack . BLC.unpack . fromJust $ r ^? responseBody
            ips = map (T.intercalate ".") . group4 . T.lines $ rb
            addrs = filter (\ip -> not (ipReserved ip) && pred ip) ips
 #ifdef DEBUG
        if length addrs /= length ips
            then do
                putStrLn "Filtered addresses:"
                print $ filter ipReserved ips
            else return ()
 #endif
        tailIps <- gen (count - (fromIntegral $ length addrs)) pred
        return $ addrs ++ tailIps 

    url = "https://www.random.org/integers"

    -- | See parameters explanation in: https://www.random.org/clients/http/.
    opts num = defaults
        & param "num"    .~ [T.pack $ show num]
        & param "min"    .~ ["0"]
        & param "max"    .~ ["255"]
        & param "col"    .~ ["1"]
        & param "base"   .~ ["10"]
        & param "format" .~ ["plain"]
        & param "rnd"    .~ ["new"]

    group4 xs
        | length xs >= 4 = loop $ drop (length xs `rem` 4) xs
        | otherwise      = error "Input list is too small (< 4)."
      where
        loop [] = []
        loop xs = take 4 xs : loop (drop 4 xs)

    evenlyCombine xs [] = xs
    evenlyCombine [] ys = ys
    evenlyCombine xs ys =
        if length xs >= length ys
            then merge xs ys (length xs `div` length ys)
            else merge ys xs (length ys `div` length xs)
      where
        merge [] ys _ = ys
        merge xs [] _ = xs
        merge xs ys n = take n xs ++ [head ys] ++ merge (drop n xs) (tail ys) n

randomAddrs :: Integer -> FilePath -> IO ()
randomAddrs numAddrs outputFile =
    genAddrs numAddrs Nothing outputFile

matchingAddrs :: Integer -> FilePath -> Double -> FilePath -> IO ()
matchingAddrs numAddrs pfxsFile ratio outputFile =
    genAddrs numAddrs (Just (pfxsFile, ratio)) outputFile

-- | WARNING: This algorithm is EXTREMELY SLOW (due to the naive lookup/check)
-- and the folks at 'http://random.org' block your IP address if you try to get
-- too many numbers.
main :: IO ()
main = sequence_ $ do
        (r, f) <- zip ratios outputFiles
        return $ do
            putStrLn $ "Generating " ++ show (r, f) ++ "..."
            matchingAddrs numAddrs pfxsFile r f
            --randomAddrs numAddrs f
  where
    numAddrs = 2^26
    pfxsFile = "/Users/alexandrelucchesi/Development/c/ip-datasets/ipv4/basic/as65000/bgptable/prefixes.txt"
    ratios = [ 0.5 ]
--    ratios = [ 0.0, 0.25, 0.5, 0.75, 1.0 ]
    outputFiles = [ "result/matching-" ++ show (round $ r * 100) ++ ".txt"
                    | r <- ratios ]

