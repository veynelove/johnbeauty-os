docs/images/ — 图片目录（预留）
====================================================

当前文档里的架构图方案（三层兼容）：
  1) 默认显示：ASCII 架构图
        - 纯文本，GitLab/GitHub/任何编辑器原生可看
        - 文字清晰，没有"放大后看不清"问题
        - 没有渲染器导致的大片留白
  2) 源码保留：每个 ASCII 图下方有 <details> 折叠块存原始 Mermaid
        - 将来本机装了 node + @mermaid-js/mermaid-cli (mmdc)
          就可以把折叠块里的代码抽出来导出大图 SVG：
              mmdc -i xxx.mmd -o jlos_arch_overview.svg -w 2400 -b white
        - 然后把生成的 SVG 放到本目录，用 markdown:
              ![图说明](./images/xxx.svg)
          这样点图即可单独打开缩放。

未来如果装了 mmdc，导出命令示例：
  $ npm i -g @mermaid-js/mermaid-cli
  $ mmdc --version   # 应该输出 10.x+
  $ mmdc -i input.mmd -o docs/images/overview.svg \
         -w 2560 -b transparent -s 2   # 2 倍屏清晰
