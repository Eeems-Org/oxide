# Application Compatibility

| Application  | Works with display server | Works with tarnish |
| ------------- | ------------- | ------------- |
| KOReader | :heavy_check_mark:  | :heavy_check_mark: <sup>6</sup> |
| DOOMarkable | :heavy_check_mark:  | :heavy_check_mark: |
| recrossable | :heavy_check_mark:  | :heavy_check_mark: |
| Plato | :heavy_check_mark:  | :heavy_check_mark: |
| Puzzles | :heavy_check_mark:  | :heavy_check_mark: |
| Sill | :heavy_check_mark:<sup>4</sup>  | :heavy_check_mark:<sup>4</sup> |
| TilEm | :heavy_check_mark:  | :heavy_check_mark: |
| Calculator | :heavy_check_mark:  | :heavy_check_mark: |
| ChessMarkable | :heavy_check_mark:  | :heavy_check_mark: |
| harmony | :heavy_check_mark: | :heavy_check_mark: |
| keywriter | :heavy_check_mark:  | :heavy_check_mark: |
| nao | :heavy_check_mark: <sup>1</sup> | :heavy_check_mark: <sup>1</sup> |
| rmFm | :heavy_check_mark: <sup>1</sup> | :heavy_check_mark: <sup>1</sup> |
| xochitl | :x: <sup>2</sup> | :x: <sup>2</sup> |
| whiteboard-hypercard | :x:<sup>3</sup>  | :x:<sup>3</sup> |
| wikipedia | :x:<sup>8</sup> | :x:<sup>8</sup> |
| yaft | :heavy_check_mark:  | :heavy_check_mark: |
| dumbskull | :heavy_check_mark: | :heavy_check_mark: |
| mines | :heavy_check_mark: | :heavy_check_mark: |
| wordlet | :heavy_check_mark: | :heavy_check_mark: |
| rpncalc | :heavy_check_mark: | :heavy_check_mark: |
| netsurf | :heavy_check_mark: | :heavy_check_mark: |
| reterm | :x:<sup>5</sup> | :x:<sup>5</sup> |
| folly | :heavy_check_mark:<sup>4</sup> | :heavy_check_mark:<sup>4</sup> |
| sudoku | :heavy_check_mark: | :heavy_check_mark: |
| fingerterm | :x:<sup>7</sup> | :x:<sup>7</sup> |

1. The surface closes between screens and shows the app behind it
2. Some UI display issues, and drawing will not update the display
3. Crashes entire display server with bad_alloc
4. Drawing lags behind very noticeably
5. Application crashes
6. Requires updating the application registration to the following:
```json
{
  "displayName": "KOReader",
  "description": "An ebook reader application supporting PDF, DjVu, EPUB, FB2 and many more formats",
  "bin": "/opt/koreader/reader.lua",
  "icon": "/opt/etc/draft/icons/koreader.png",
  "type": "foreground",
  "workingDirectory": "/opt/koreader",
  "environment": {
    "KO_DONT_GRAB_INPUT": "1",
    "KO_DONT_SET_DEPTH": "1",
    "TESSDATA_PREFIX": "data",
    "STARDICT_DATA_DIR": "data/dict"
  }
}
```
7. Many issues:
 - Will often freeze the display server and crash the application.
 - Unable to open side menu
 - `OXIDE_PRELOAD_FORCE_QT=1`
   - Side menu will now open
   - will have some of the UI rotate, but not the right direction.
   - The terminal itself will still display as if it's not rotated.
   - Will complain about an unknown special key `16777249` when pressing left ctrl
   - Will fail to start bash properly often will leave you unable to run any commands.
8. Display renders incorrectly with areas left blank or not updating after you click the first link.
