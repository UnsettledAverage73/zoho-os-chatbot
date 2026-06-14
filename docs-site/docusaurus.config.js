// @ts-check

const config = {
  title: "Zoho OS Documentation",
  tagline: "A clean guide to the boot flow, kernel architecture, and user space",
  url: "https://example.com",
  baseUrl: "/",
  organizationName: "zoho-os",
  projectName: "zoho-os-docs",
  onBrokenLinks: "warn",
  markdown: {
    hooks: {
      onBrokenMarkdownLinks: "warn"
    }
  },
  i18n: {
    defaultLocale: "en",
    locales: ["en"]
  },
  presets: [
    [
      "classic",
      {
        docs: {
          routeBasePath: "/",
          sidebarPath: require.resolve("./sidebars.js")
        },
        blog: false,
        pages: false,
        theme: {
          customCss: require.resolve("./src/css/custom.css")
        }
      }
    ]
  ],
  themeConfig: {
    colorMode: {
      defaultMode: "light",
      disableSwitch: false,
      respectPrefersColorScheme: true
    },
    navbar: {
      title: "Zoho OS",
      hideOnScroll: true,
      items: [
        {
          to: "/",
          position: "left",
          label: "Home"
        },
        {
          to: "/overview",
          position: "left",
          label: "Overview"
        },
        {
          type: "docSidebar",
          sidebarId: "tutorialSidebar",
          position: "left",
          label: "Boot Sequence"
        },
        {
          to: "/architecture",
          position: "left",
          label: "Architecture"
        },
        {
          to: "/build-and-run",
          position: "left",
          label: "Build"
        },
        {
          href: "https://github.com/UnsettledAverage73/zoho-os",
          label: "Source",
          position: "right"
        }
      ]
    },
    footer: {
      style: "light",
      links: [
        {
          title: "Docs",
          items: [
            {
              label: "Overview",
              to: "/overview"
            },
            {
              label: "Boot Sequence",
              to: "/boot/00-overview"
            },
            {
              label: "Architecture",
              to: "/architecture"
            }
          ]
        },
        {
          title: "Project",
          items: [
            {
              label: "Build and Run",
              to: "/build-and-run"
            },
            {
              label: "Source Code",
              href: "https://github.com/UnsettledAverage73/zoho-os"
            }
          ]
        }
      ],
      copyright: `Copyright ${new Date().getFullYear()} Zoho OS`
    }
  }
};

module.exports = config;
